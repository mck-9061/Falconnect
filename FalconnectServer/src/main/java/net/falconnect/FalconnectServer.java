package net.falconnect;

import net.falconnect.messages.toclient.ConnectedMessage;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.List;

public class FalconnectServer {
  public static final int UDP_PORT_BASE = 9000;
  // Reserve a full grid per game, independent of the configurable admission limit.
  public static final int UDP_PORTS_PER_RACE = RaceDataFormat.MAX_RACERS;
  public static final int MAX_SUPPORTED_RACES = (UDP_PORT_BASE - 1024) / UDP_PORTS_PER_RACE;

  private volatile int maxClientsPerRace = 6;
  private volatile int maxConcurrentRaces = 3;
  private volatile int maxCpusPerClient = 3;
  private volatile int countdownSeconds = 20;

  public ServerSocket socket;
  private final List<FalconnectRace> currentRaces = new ArrayList<>();
  private AcceptClientConnectionThread acceptClientConnectionThread;
  private ConsoleCommandHandlerThread consoleCommandHandlerThread;

  public FalconnectServer() throws IOException {
    this(null);
    socket = new ServerSocket(8000);
    System.out.println("Server socket created");

    acceptClientConnectionThread = new AcceptClientConnectionThread(this);
    acceptClientConnectionThread.start();

    consoleCommandHandlerThread = new ConsoleCommandHandlerThread(this);
    consoleCommandHandlerThread.start();
  }

  // Separate construction from listener startup for embedded use and command tests.
  FalconnectServer(ServerSocket socket) {
    // Validate every editable table row before starting any network threads.
    TrackTable.get(0);
    this.socket = socket;
  }

  public int getMaxClientsPerRace() { return maxClientsPerRace; }
  public int getMaxConcurrentRaces() { return maxConcurrentRaces; }
  public int getMaxCpusPerClient() { return maxCpusPerClient; }

  public int getCountdownSeconds() { return countdownSeconds; }

  public void setCountdownSeconds(int value) {
    requireRange(value, 1, 3600, "Countdown seconds");
    countdownSeconds = value;
  }

  public TrackSettings getTrackSettings(int trackId) { return TrackTable.get(trackId); }

  public synchronized void setMaxClientsPerRace(int value) {
    requireRange(value, 2, RaceDataFormat.MAX_RACERS, "Maximum players per game");
    maxClientsPerRace = value;
  }

  public synchronized void setMaxConcurrentRaces(int value) {
    requireRange(value, 1, MAX_SUPPORTED_RACES, "Maximum concurrent games");
    maxConcurrentRaces = value;
  }

  public void setMaxCpusPerClient(int value) {
    requireRange(value, 0, RaceDataFormat.MAX_RACERS - 1, "Maximum CPUs per client");
    maxCpusPerClient = value;
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
