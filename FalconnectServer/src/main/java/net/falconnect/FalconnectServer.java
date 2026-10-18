package net.falconnect;

import net.falconnect.messages.toclient.ConnectedMessage;

import java.io.IOException;
import java.net.ServerSocket;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

public class FalconnectServer {
  public static final int UDP_PORT_BASE = 9000;
  // Reserve a full grid per game, independent of the configurable admission limit.
  public static final int UDP_PORTS_PER_RACE = RaceDataFormat.MAX_RACERS;
  public static final int MAX_SUPPORTED_RACES = (UDP_PORT_BASE - 1024) / UDP_PORTS_PER_RACE;
  public static final Path DEFAULT_CONFIGURATION_PATH = Path.of("falconnect-server.properties");

  private volatile int maxClientsPerRace = 6;
  private volatile int maxConcurrentRaces = 3;
  private volatile int maxCpusPerClient = 3;
  private volatile int countdownSeconds = 20;
  private volatile int trackVoteCountdownSeconds = 20;

  public ServerSocket socket;
  private final List<FalconnectRace> currentRaces = new ArrayList<>();
  private final Path configurationPath;
  private AcceptClientConnectionThread acceptClientConnectionThread;
  private ConsoleCommandHandlerThread consoleCommandHandlerThread;

  public FalconnectServer() throws IOException {
    this(null, DEFAULT_CONFIGURATION_PATH);
    socket = new ServerSocket(8000);
    System.out.println("Server socket created");

    acceptClientConnectionThread = new AcceptClientConnectionThread(this);
    acceptClientConnectionThread.start();

    consoleCommandHandlerThread = new ConsoleCommandHandlerThread(this);
    consoleCommandHandlerThread.start();
  }

  // Separate construction from listener startup for embedded use and command tests.
  FalconnectServer(ServerSocket socket) {
    this(socket, null);
  }

  // A supplied path is used by configuration tests and embedded deployments. Null keeps the
  // in-memory-only behaviour needed by command tests.
  FalconnectServer(ServerSocket socket, Path configurationPath) {
    // Validate every editable table row before starting any network threads.
    TrackTable.get(0);
    this.socket = socket;
    this.configurationPath = configurationPath;
    if (configurationPath != null) loadConfiguration();
  }

  public int getMaxClientsPerRace() { return maxClientsPerRace; }
  public int getMaxConcurrentRaces() { return maxConcurrentRaces; }
  public int getMaxCpusPerClient() { return maxCpusPerClient; }

  public int getCountdownSeconds() { return countdownSeconds; }
  public int getTrackVoteCountdownSeconds() { return trackVoteCountdownSeconds; }

  public void setCountdownSeconds(int value) {
    requireRange(value, 1, 3600, "Countdown seconds");
    countdownSeconds = value;
    saveConfiguration();
  }

  public void setTrackVoteCountdownSeconds(int value) {
    requireRange(value, 1, 3600, "Track vote countdown seconds");
    trackVoteCountdownSeconds = value;
    saveConfiguration();
  }

  public TrackSettings getTrackSettings(int trackId) { return TrackTable.get(trackId); }

  public synchronized void setMaxClientsPerRace(int value) {
    requireRange(value, 2, RaceDataFormat.MAX_RACERS, "Maximum players per game");
    maxClientsPerRace = value;
    saveConfiguration();
  }

  public synchronized void setMaxConcurrentRaces(int value) {
    requireRange(value, 1, MAX_SUPPORTED_RACES, "Maximum concurrent games");
    maxConcurrentRaces = value;
    saveConfiguration();
  }

  public void setMaxCpusPerClient(int value) {
    requireRange(value, 0, RaceDataFormat.MAX_RACERS - 1, "Maximum CPUs per client");
    maxCpusPerClient = value;
    saveConfiguration();
  }

  private void loadConfiguration() {
    if (!Files.exists(configurationPath)) {
      saveConfiguration();
      System.out.println("Created server configuration at " + configurationPath.toAbsolutePath());
      return;
    }

    Properties properties = new Properties();
    try (var reader = Files.newBufferedReader(configurationPath)) {
      properties.load(reader);
      int configuredMaxPlayers = configurationValue(properties, "max_players_per_race",
          maxClientsPerRace);
      int configuredMaxRaces = configurationValue(properties, "max_concurrent_races",
          maxConcurrentRaces);
      int configuredMaxCpus = configurationValue(properties, "max_cpus_per_client",
          maxCpusPerClient);
      int configuredCountdown = configurationValue(properties, "ready_countdown_seconds",
          countdownSeconds);
      int configuredTrackCountdown = configurationValue(properties, "track_vote_countdown_seconds",
          trackVoteCountdownSeconds);

      requireRange(configuredMaxPlayers, 2, RaceDataFormat.MAX_RACERS,
          "Configured maximum players per game");
      requireRange(configuredMaxRaces, 1, MAX_SUPPORTED_RACES,
          "Configured maximum concurrent games");
      requireRange(configuredMaxCpus, 0, RaceDataFormat.MAX_RACERS - 1,
          "Configured maximum CPUs per client");
      requireRange(configuredCountdown, 1, 3600, "Configured ready countdown seconds");
      requireRange(configuredTrackCountdown, 1, 3600,
          "Configured track vote countdown seconds");

      maxClientsPerRace = configuredMaxPlayers;
      maxConcurrentRaces = configuredMaxRaces;
      maxCpusPerClient = configuredMaxCpus;
      countdownSeconds = configuredCountdown;
      trackVoteCountdownSeconds = configuredTrackCountdown;
      System.out.println("Loaded server configuration from " + configurationPath.toAbsolutePath());
    } catch (IOException | IllegalArgumentException e) {
      System.out.println("Ignoring invalid server configuration at " + configurationPath.toAbsolutePath()
          + ": " + e.getMessage());
    }
  }

  private static int configurationValue(Properties properties, String key, int defaultValue) {
    String value = properties.getProperty(key);
    return value == null ? defaultValue : Integer.parseInt(value.trim());
  }

  private synchronized void saveConfiguration() {
    if (configurationPath == null) return;

    Properties properties = new Properties();
    properties.setProperty("max_players_per_race", Integer.toString(maxClientsPerRace));
    properties.setProperty("max_concurrent_races", Integer.toString(maxConcurrentRaces));
    properties.setProperty("max_cpus_per_client", Integer.toString(maxCpusPerClient));
    properties.setProperty("ready_countdown_seconds", Integer.toString(countdownSeconds));
    properties.setProperty("track_vote_countdown_seconds", Integer.toString(trackVoteCountdownSeconds));

    Path absolutePath = configurationPath.toAbsolutePath();
    Path parent = absolutePath.getParent();
    try {
      if (parent != null) Files.createDirectories(parent);
      Path temporaryPath = Files.createTempFile(parent, "falconnect-server-", ".properties");
      try (var writer = Files.newBufferedWriter(temporaryPath)) {
        properties.store(writer, " Falconnect server settings");
      }
      try {
        Files.move(temporaryPath, absolutePath, StandardCopyOption.ATOMIC_MOVE,
            StandardCopyOption.REPLACE_EXISTING);
      } catch (AtomicMoveNotSupportedException e) {
        Files.move(temporaryPath, absolutePath, StandardCopyOption.REPLACE_EXISTING);
      }
    } catch (IOException e) {
      System.out.println("Unable to save server configuration to " + absolutePath + ": "
          + e.getMessage());
    }
  }

  static void requireRange(int value, int min, int max, String name) {
    if (value < min || value > max)
      throw new IllegalArgumentException(name + " must be between " + min + " and " + max + ".");
  }

  public synchronized List<FalconnectRace> getRaces() {
    return List.copyOf(currentRaces);
  }

  public synchronized void setTrackOverride(int gameId, Integer trackId) {
    findGame(gameId).setTrackOverride(trackId);
  }

  public synchronized void setLapCountOverride(int gameId, Integer lapCount) {
    findGame(gameId).setLapCountOverride(lapCount);
  }

  public synchronized void setBoostLapOverride(int gameId, Integer boostLap) {
    findGame(gameId).setBoostLapOverride(boostLap);
  }

  public synchronized void setMachineOverride(int gameId, MachineSelection selection) {
    findGame(gameId).setMachineOverride(selection);
  }

  public synchronized int selectRandomTrack(int gameId) {
    FalconnectRace race = findGame(gameId);
    race.selectRandomTrack();
    return race.getTrackOverride();
  }

  public synchronized void skipTrackVoteCountdown(int gameId) { findGame(gameId).skipTrackVoteCountdown(); }
  public synchronized void skipReadyCountdown(int gameId) { findGame(gameId).skipReadyCountdown(); }

  private FalconnectRace findGame(int gameId) {
    for (FalconnectRace race : currentRaces) {
      if (race.getPortBlockIndex() == gameId) return race;
    }
    throw new IllegalArgumentException("No game with ID " + gameId + ". Use list to see game IDs.");
  }

  public synchronized boolean kickPlayer(int uid) {
    for (FalconnectRace race : currentRaces) {
      for (FalconnectClientConnection client : race.getClients()) {
        if (client.uid == uid) {
          client.Disconnect("You have been kicked from the server.");
          return true;
        }
      }
    }
    return false;
  }

  public synchronized int closeAllGames() {
    int count = currentRaces.size();
    for (FalconnectRace race : currentRaces) {
      race.Close();
      for (FalconnectClientConnection client : race.getClients())
        client.Disconnect("All games have been closed by the server administrator.");
    }
    currentRaces.clear();
    return count;
  }

  // A normal JVM shutdown (including Ctrl-C) should follow the same control path as kickall so
  // connected clients receive a reason before TCP reaches EOF.  A hard kill still relies on the
  // client's EOF handling.
  public synchronized void shutdown() {
    closeAllGames();
    if (socket != null && !socket.isClosed()) {
      try {
        socket.close();
      } catch (IOException e) {
        System.out.println("Unable to close server socket: " + e.getMessage());
      }
    }
  }

  public synchronized void RemoveRace(FalconnectRace race) {
    currentRaces.remove(race);
  }

  public synchronized boolean RemoveRaceIfEmpty(FalconnectRace race) {
    if (!race.IsClientsEmpty()) return false;
    race.Close();
    currentRaces.remove(race);
    return true;
  }

  public synchronized void EndRace(FalconnectRace race) throws IOException, InterruptedException {
    // A closed game must not reallocate its players or recreate itself after kickall.
    if (!currentRaces.remove(race)) return;
    System.out.println("Race ended, re-allocating clients");
    List<FalconnectClientConnection> clients = new ArrayList<>(race.getClients());

    for (FalconnectClientConnection client : clients) {
      client.CloseUdpSocket();
      if (client.disconnected) continue;
      AllocateClient(client);
    }
  }

  public synchronized void AllocateClient(FalconnectClientConnection client) throws IOException, InterruptedException {
    if (client.disconnected) return;
    System.out.println("Allocating " + client.socket.getRemoteSocketAddress() + "...");

    // Reset and EndRace can both attempt to allocate the same live connection. The race that
    // wins first is already the client's new lobby, so a later attempt must be a no-op rather
    // than appending a second copy (or placing it in another lobby).
    for (FalconnectRace race : currentRaces) {
      if (race.HasClient(client)) {
        client.setRace(race);
        System.out.println("Client is already allocated to an existing race");
        return;
      }
    }

    if (currentRaces.isEmpty()) {
    //if (false) {
      // Start a new race
      FalconnectRace race = CreateRace();
      currentRaces.add(race);
      race.AddClient(client);
      race.start();
      System.out.println("Allocated to new race (no current races)");
      SendConnectedMessage(client);
      return;
    }

    for (FalconnectRace race : currentRaces) {
      if (race.TryAddClient(client)) {
        System.out.println("Allocated to existing race");
        SendConnectedMessage(client);
        return;
      }
    }

    if (currentRaces.size() < maxConcurrentRaces) {
      // Start a new race
      FalconnectRace race = CreateRace();
      currentRaces.add(race);
      race.AddClient(client);
      race.start();
      System.out.println("Allocated to new race");
      SendConnectedMessage(client);
      return;
    }

    System.out.println("No space!");

    // No space anywhere for client :(
    client.Disconnect("This server is full. Please try again later.");
  }

  private FalconnectRace CreateRace() throws IOException {
    for (int portBlockIndex = 0; portBlockIndex < MAX_SUPPORTED_RACES; portBlockIndex++) {
      boolean inUse = false;
      for (FalconnectRace race : currentRaces) {
        if (race.getPortBlockIndex() == portBlockIndex) {
          inUse = true;
          break;
        }
      }

      if (!inUse) return new FalconnectRace(this, portBlockIndex);
    }

    throw new IOException("No UDP port block is available for a new race");
  }

  private void SendConnectedMessage(FalconnectClientConnection client) throws InterruptedException {
    new ConnectedMessage(client).Send();
  }
}
