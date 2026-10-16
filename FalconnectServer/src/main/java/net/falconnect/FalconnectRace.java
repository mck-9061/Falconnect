package net.falconnect;

import net.falconnect.messages.toclient.*;

import java.io.IOException;
import java.util.*;

public class FalconnectRace extends Thread {
  private static final long UDP_TIMEOUT_MILLIS = 5_000;
  private static final long CPU_HANDOFF_TIMEOUT_MILLIS = 750;
  private int maxCpusPerClient;
  private Integer trackOverride;
  private MachineSelection machineOverride;
  private List<MachineSelection.Machine> raceMachines;

  private Integer lapCountOverride;
  private Integer boostLapOverride;
  private String lastSettingsError;
  private TrackSettings selectedTrackSettings;
  private static final int REGULAR_MACHINE_COUNT = 30;
  private List<FalconnectClientConnection> clients;
  private FalconnectServer server;
  private final int portBlockIndex;

  private byte[] fullDataPacket;
  private byte[] cpuRacerIds;
  private Long firstReadyAtNanos;
  private int lastLobbyPlayerCount = -1;
  private int lastLobbyReadyCount = -1;

  public volatile GameState gameState;
  public volatile boolean isRunning;

  public FalconnectRace(FalconnectServer server, int portBlockIndex) throws IOException {
    clients = new ArrayList<>();
    gameState = GameState.WAITING_FOR_READY;

    this.server = server;
    this.portBlockIndex = portBlockIndex;
    isRunning = true;

    //HackyAssSoftlockPreventionThread hackyAssSoftlockPreventionThread = new HackyAssSoftlockPreventionThread(this);
    //hackyAssSoftlockPreventionThread.start();
  }

  public synchronized boolean HasSpace() {
    return isRunning && clients.size() < server.getMaxClientsPerRace();
  }

  public int getPortBlockIndex() {
    return portBlockIndex;
  }

  public int getUdpPort(byte playerNum) {
    return FalconnectServer.UDP_PORT_BASE
        - (portBlockIndex * FalconnectServer.UDP_PORTS_PER_RACE)
        - playerNum;
  }

  public synchronized boolean IsClientsEmpty() {
    return clients.isEmpty();
  }

  public synchronized List<FalconnectClientConnection> getClients() {
    return List.copyOf(clients);
  }

  public synchronized void setMachineOverride(MachineSelection selection) {
    requireWaitingForReady();
    machineOverride = selection;
  }

  public synchronized MachineSelection getMachineOverride() { return machineOverride; }

  public synchronized void selectRandomTrack() {
    requireWaitingForReady();
    List<Integer> candidates = new ArrayList<>();
    for (int id : TrackTable.trackIds()) {
      try {
        resolveTrackSettings(id, lapCountOverride, boostLapOverride);
        candidates.add(id);
      } catch (IllegalArgumentException ignored) {
        // Only draw from tracks compatible with the game's explicit lap settings.
      }
    }
    if (candidates.isEmpty())
      throw new IllegalArgumentException("No configured tracks match these lap/boost overrides.");
    setTrackOverride(candidates.get(new Random().nextInt(candidates.size())));
  }

  public synchronized MachineSelection.Machine getRaceMachine(int index) {
    return raceMachines.get(index);
  }

  public synchronized void setTrackOverride(Integer trackId) {
    requireWaitingForReady();
    if (trackId != null) FalconnectServer.requireRange(trackId, 0, 255, "Track ID");
    validateOverrides(trackId, lapCountOverride, boostLapOverride);
    trackOverride = trackId;
  }

  public synchronized Integer getTrackOverride() { return trackOverride; }
  public synchronized Integer getLapCountOverride() { return lapCountOverride; }
  public synchronized Integer getBoostLapOverride() { return boostLapOverride; }

  public synchronized void setLapCountOverride(Integer lapCount) {
    requireWaitingForReady();
    if (lapCount != null) FalconnectServer.requireRange(lapCount, 1, 255, "Lap count");
    validateOverrides(trackOverride, lapCount, boostLapOverride);
    lapCountOverride = lapCount;
  }

  public synchronized void setBoostLapOverride(Integer boostLap) {
    requireWaitingForReady();
    if (boostLap != null) FalconnectServer.requireRange(boostLap, 0, 255, "Boost lap");
    validateOverrides(trackOverride, lapCountOverride, boostLap);
    boostLapOverride = boostLap;
  }

  private void requireWaitingForReady() {
    if (!isRunning || gameState != GameState.WAITING_FOR_READY)
      throw new IllegalArgumentException("Race settings can only be changed while the game is waiting for players to ready up.");
  }

  private void validateOverrides(Integer trackId, Integer laps, Integer boost) {
    if (trackId != null) resolveTrackSettings(trackId, laps, boost);
    else if (laps != null && boost != null)
      FalconnectServer.requireRange(boost, 0, laps, "Boost lap");
  }

  private TrackSettings resolveTrackSettings(int trackId, Integer laps, Integer boost) {
    TrackSettings track = server.getTrackSettings(trackId);
    int lapCount = laps == null ? track.lapCount() : laps;
    int boostLap = boost == null ? track.boostLap() : boost;
    if (boostLap > lapCount)
      throw new IllegalArgumentException("Boost lap " + boostLap + " exceeds lap count "
          + lapCount + " for track " + trackId + ".");
    return new TrackSettings(trackId, track.name(), lapCount, boostLap);
  }

  public synchronized TrackSettings getSelectedTrackSettings() { return selectedTrackSettings; }

  public synchronized void Close() {
    isRunning = false;
    interrupt();
  }

  // Capture settings and select the course under the same lock as console track overrides.
  // This limit is retained through CPU handoff/restoration for the whole race.
  synchronized Byte PrepareRaceStart() {
    return PrepareRaceStart(System.nanoTime());
  }

  synchronized Byte PrepareRaceStart(long nowNanos) {
    if (!isRunning || gameState != GameState.WAITING_FOR_READY) return null;
    boolean anyReady = clients.stream().anyMatch(client ->
        !client.disconnected && client.state == ClientState.READY);
    if (!anyReady) {
      firstReadyAtNanos = null;
      return null;
    }
    if (firstReadyAtNanos == null) firstReadyAtNanos = nowNanos;
    boolean countdownExpired = nowNanos - firstReadyAtNanos >=
        server.getCountdownSeconds() * 1_000_000_000L;
    boolean allReady = clients.size() >= 2 && clients.stream().allMatch(client ->
        !client.disconnected && client.state == ClientState.READY);
    if (!allReady && !countdownExpired) return null;
    if (clients.stream().anyMatch(client -> client.disconnected)) return null;

    try {
      List<TrackSettings> courses = new ArrayList<>();
      if (trackOverride != null) {
        courses.add(resolveTrackSettings(trackOverride, lapCountOverride, boostLapOverride));
      } else {
        // Votes may change after a command. Validate every candidate before choosing so an
        // invalid combination neither kills the race thread nor silently changes the vote odds.
        for (FalconnectClientConnection client : clients) {
          if (client.state != ClientState.READY) continue;
          courses.add(resolveTrackSettings(Byte.toUnsignedInt(client.selectedCourse),
              lapCountOverride, boostLapOverride));
        }
      }
      selectedTrackSettings = courses.get(new Random().nextInt(courses.size()));
      lastSettingsError = null;
    } catch (IllegalArgumentException e) {
      String error = "Cannot start game " + portBlockIndex + ": " + e.getMessage()
          + " Adjust laps/boostlap overrides or the track vote.";
      if (!error.equals(lastSettingsError)) System.out.println(error);
      lastSettingsError = error;
      return null;
    }
    maxCpusPerClient = server.getMaxCpusPerClient();
    int totalCpus = Math.min(maxCpusPerClient * clients.size(),
        RaceDataFormat.MAX_RACERS - clients.size());
    int baseCpuCount = totalCpus / clients.size();
    int remainder = totalCpus % clients.size();
    int nextCpuIndex = clients.size();
    byte playerNum = 1;
    for (FalconnectClientConnection client : clients) {
      // Players without submitted settings use the same standard machine locally and remotely.
      if (client.state != ClientState.READY) {
        client.racerId = 6;
        Arrays.fill(client.customMachineData, (byte) 0);
      }
      client.state = ClientState.LOADING;
      client.playerNum = playerNum++;
      int cpuCount = baseCpuCount + (client.playerNum <= remainder ? 1 : 0);
      List<Byte> cpuIndices = new ArrayList<>();
      for (int i = 0; i < cpuCount; i++) cpuIndices.add((byte) nextCpuIndex++);
      client.setHomeCpuRacerIndices(cpuIndices);
      client.cpuStartIndex = cpuIndices.isEmpty() ? 0 : cpuIndices.get(0);
      client.udpPort = getUdpPort(client.playerNum);
    }
    GenerateCpuRacerIds();
    if (machineOverride != null) {
      raceMachines = machineOverride.generate();
    } else {
      List<MachineSelection.Machine> grid = new ArrayList<>();
      for (FalconnectClientConnection client : clients)
        grid.add(new MachineSelection.Machine(client.usesCustomMachine() ? (byte) 0x32 : client.racerId,
            client.customMachineData[0], client.customMachineData[1], client.customMachineData[2]));
      for (int i = clients.size(); i < RaceDataFormat.MAX_RACERS; i++)
        grid.add(new MachineSelection.Machine(cpuRacerIds[i], (byte) 0, (byte) 0, (byte) 0));
      raceMachines = List.copyOf(grid);
    }
    gameState = GameState.WAITING_FOR_GRID;
    return (byte) selectedTrackSettings.trackId();
  }

  public synchronized void GenerateCpuRacerIds() {
    cpuRacerIds = new byte[RaceDataFormat.MAX_RACERS];
    Random random = new Random();
    for (int racerIndex = 0; racerIndex < RaceDataFormat.MAX_RACERS; racerIndex++) {
      // Preserve the default CPU pool; explicit random overrides use all standard IDs.
      cpuRacerIds[racerIndex] = (byte) (random.nextInt(REGULAR_MACHINE_COUNT) + 1);
    }
  }

  public synchronized byte getCpuRacerId(int racerIndex) {
    return cpuRacerIds[racerIndex];
  }

  public synchronized void AddClient(FalconnectClientConnection client) {
    client.udpPort = getUdpPort(client.playerNum);
    clients.add(client);
  }

  public synchronized boolean TryAddClient(FalconnectClientConnection client) {
    if (!HasSpace() || gameState != GameState.WAITING_FOR_READY) return false;
    AddClient(client);
    return true;
  }

  public synchronized void RemoveClient(FalconnectClientConnection client) {
    client.CloseUdpSocket();

    // Recreate list without dead client
    List<FalconnectClientConnection> newList = new ArrayList<>();

    for (FalconnectClientConnection c : clients) {
      if (c != client) newList.add(c);
    }

    clients = newList;
    System.out.println(clients.size());
  }

  public void run() {
    long time = System.nanoTime();
    try {
      while (isRunning) {
        // wait for half a frame to pass
        long currentTime = System.nanoTime();
        if (currentTime - time < 8333333) continue;

        time = currentTime;


        try {
          // Wait for all players to be ready
          // System.out.println("???");

          if (IsClientsEmpty() && server.RemoveRaceIfEmpty(this)) return;

          // System.out.println(gameState);

          if (gameState == GameState.WAITING_FOR_READY) {
            RemoveDisconnectedClients();
            BroadcastLobbyStatusIfChanged();
            Byte usedCourse = PrepareRaceStart();
            if (usedCourse != null) {
              System.out.println("Starting " + selectedTrackSettings.name() + " (track "
                  + selectedTrackSettings.trackId() + "): " + selectedTrackSettings.lapCount()
                  + " laps, boost on lap " + selectedTrackSettings.boostLap());
              int totalCpus = getClients().stream().mapToInt(client -> client.numCpus).sum();
              for (FalconnectClientConnection client : getClients()) {
                client.RebindUdpSocket();
                new ConnectedMessage(client).Send();
              }

              Thread.sleep(1000);

              for (FalconnectClientConnection client : getClients()) {
                new CpuAssignmentMessage(client).Send();
                // GX's course setting is the number of opponent slots to initialise. That includes
                // remote players as well as CPUs, but excludes this client's local racer.
                int opponentCount = getClients().size() + totalCpus - 1;
                CourseMessage courseMessage = new CourseMessage(client, selectedTrackSettings,
                    (byte) opponentCount);
                Thread.sleep(20);
                courseMessage.Send();
                RacerIdsMessage racerIdsMessage = new RacerIdsMessage(client, getClients(), this);
                Thread.sleep(20);
                racerIdsMessage.Send();
                CustomMachineDataMessage customMachineDataMessage =
                    new CustomMachineDataMessage(client, this);
                Thread.sleep(20);
                customMachineDataMessage.Send();
                NamesMessage namesMessage = new NamesMessage(client, getClients());
                Thread.sleep(20);
                namesMessage.Send();
                StatusMessage message = new StatusMessage(client, ToClientPacketType.START);
                Thread.sleep(20);
                message.Send();

              }
            }
          }

          // Wait for all players to be gridded
          if (gameState == GameState.WAITING_FOR_GRID) {
            RemoveDisconnectedClients();
            boolean allGridded = true;
            for (FalconnectClientConnection client : getClients()) {
              // System.out.printf("Client %s - %s", client.playerNum, client.state);
              if (client.state != ClientState.GRIDDED) {
                allGridded = false;
                break;
              }
            }

            if (allGridded) {
              System.out.println("All clients gridded; sending START_RACE to " + getClients().size() + " clients");
              gameState = GameState.RACING;
              for (FalconnectClientConnection client : getClients()) {
                client.lastUdpPacketReceivedAt = System.currentTimeMillis();
                client.resetRaceDataTracking();
                System.out.println("Sending START_RACE to player " + client.playerNum);
                StatusMessage message = new StatusMessage(client, ToClientPacketType.START_RACE);
                message.Send();
              }
            }
          }

          // Send machine data
          if (gameState == GameState.RACING) {
            RebalanceCpuAssignments();
            DisconnectClientsWithRepeatedRaceData();
            DisconnectTimedOutClients();
            RemoveDisconnectedClients();

            //System.out.println("Sending data...");
            SendFullDataPacket();
            //System.out.println("Done");

            // If all players finished, end race
            boolean allDone = true;
            for (FalconnectClientConnection client : getClients()) {
              if ((client.state == ClientState.RACING ||
                client.state == ClientState.GRIDDED) && !client.disconnected) {
                allDone = false;
                break;
              }
            }

            if (allDone) {
              System.out.println("Resetting!");
              // Players move to a new lobby; this finished game must not accept joins or overrides.
              isRunning = false;

              RemoveDisconnectedClients();

              server.EndRace(this);
              return;
            }
          }
        } catch (InterruptedException | IOException e) {
          if (!isRunning) return;
          throw new RuntimeException(e);
        }
      }
    } finally {
      isRunning = false;
      server.RemoveRace(this);
    }
  }

  public void RemoveDisconnectedClients() throws InterruptedException {
    // Remove disconnected clients
    List<FalconnectClientConnection> disconnectedClients = new ArrayList<>();
    for (FalconnectClientConnection client : getClients()) {
      if (client.disconnected) disconnectedClients.add(client);
    }

    for (FalconnectClientConnection client : disconnectedClients) {
      TransferCpuAssignments(client, false);
      RemoveClient(client);
    }
  }

  private void BroadcastLobbyStatusIfChanged() throws InterruptedException {
    int playerCount = 0;
    int readyCount = 0;
    for (FalconnectClientConnection client : getClients()) {
      if (client.disconnected) continue;
      playerCount++;
      if (client.state == ClientState.READY) readyCount++;
    }

    if (playerCount == lastLobbyPlayerCount && readyCount == lastLobbyReadyCount) return;

    lastLobbyPlayerCount = playerCount;
    lastLobbyReadyCount = readyCount;
    for (FalconnectClientConnection client : getClients()) {
      if (!client.disconnected)
        new LobbyStatusMessage(client, readyCount, playerCount).Send();
    }
  }

  private void DisconnectTimedOutClients() {
    long now = System.currentTimeMillis();
    for (FalconnectClientConnection client : getClients()) {
      if (!client.disconnected && client.state != ClientState.IN_MENUS &&
          now - client.lastUdpPacketReceivedAt > UDP_TIMEOUT_MILLIS) {
        System.out.println("Client timed out during race: " + client.playerNum);
        DisconnectClient(client, "No race data received for 5 seconds.");
      }
    }
  }

  private void RebalanceCpuAssignments() throws InterruptedException {
    long now = System.currentTimeMillis();
    for (FalconnectClientConnection client : getClients()) {
      if (client.state == ClientState.IN_MENUS) {
        // A player who leaves to Course Select remains connected for the next race, but no longer
        // owns simulation work in this one.
        if (!client.getCpuRacerIndices().isEmpty()) TransferCpuAssignments(client, true);
        client.cpuRestorationNeeded = false;
        continue;
      }

      boolean hasStoppedSendingRaceData = client.hasReceivedRaceData &&
          now - client.lastUdpPacketReceivedAt > CPU_HANDOFF_TIMEOUT_MILLIS;
      if (!client.cpuHandoffActive && hasStoppedSendingRaceData &&
          !client.getCpuRacerIndices().isEmpty()) {
        TransferCpuAssignments(client, true);
      }
      if (client.cpuRestorationNeeded && client.cpuHandoffActive && !client.disconnected) {
        RestoreHomeCpuAssignment(client);
        client.cpuRestorationNeeded = false;
      }
    }
  }

  private void TransferCpuAssignments(FalconnectClientConnection source, boolean notifySource)
      throws InterruptedException {
    long now = System.currentTimeMillis();
    List<Byte> sourceCpuIndices = source.getCpuRacerIndices();
    if (sourceCpuIndices.isEmpty()) return;

    // A recipient can never be assigned more than this race's configured CPU limit. If a
    // player disappears after every remaining client reaches that limit, the surplus racers
    // deliberately remain unowned rather than making a client produce an oversized packet.
    List<Byte> pendingCpuIndices = new ArrayList<>(sourceCpuIndices);
    source.setCpuRacerIndices(new ArrayList<>());
    source.cpuHandoffActive = notifySource;

    Set<FalconnectClientConnection> changedRecipients = new HashSet<>();
    while (!pendingCpuIndices.isEmpty()) {
      FalconnectClientConnection recipient = getClients().stream()
          .filter(other -> other != source && !other.disconnected &&
              other.getCpuRacerIndices().size() < maxCpusPerClient &&
              CanReceiveCpuAssignment(other, now))
          .min(Comparator.comparingInt(other -> other.getCpuRacerIndices().size()))
          .orElse(null);
      if (recipient == null) break;

      List<Byte> recipientCpuIndices = recipient.getCpuRacerIndices();
      recipientCpuIndices.add(pendingCpuIndices.remove(0));
      recipient.setCpuRacerIndices(recipientCpuIndices);
      changedRecipients.add(recipient);
    }

    if (!pendingCpuIndices.isEmpty()) {
      System.out.println("No CPU capacity remains for " + pendingCpuIndices + " from player "
          + source.playerNum);
    }
    try {
      // These are state-changing control messages, not lossy race data. Send them immediately so
      // the recipient begins producing frames before the next server broadcast.
      if (notifySource) new CpuAssignmentMessage(source).Send();
      for (FalconnectClientConnection recipient : changedRecipients)
        new CpuAssignmentMessage(recipient).Send();
    } catch (Exception e) {
      System.out.println("Unable to deliver a CPU assignment");
    }
  }

  private boolean CanReceiveCpuAssignment(FalconnectClientConnection client, long now) {
    if (gameState == GameState.WAITING_FOR_GRID) {
      // UDP has not started yet. A connected client which has not returned to the menus is the
      // only available liveness signal while the race is loading.
      return client.state != ClientState.IN_MENUS;
    }

    // Once the race is running, UDP is the authoritative liveness signal; TCP state changes can
    // lag behind the transition to Course Select.
    return client.hasReceivedRaceData &&
        now - client.lastUdpPacketReceivedAt <= CPU_HANDOFF_TIMEOUT_MILLIS;
  }

  private void RestoreHomeCpuAssignment(FalconnectClientConnection recoveringClient) throws InterruptedException {
    List<Byte> restored = recoveringClient.getCpuRacerIndices();
    for (Byte cpuIndex : recoveringClient.getHomeCpuRacerIndices()) {
      if (restored.size() >= maxCpusPerClient) break;
      if (restored.contains(cpuIndex)) continue;
      for (FalconnectClientConnection owner : getClients()) {
        if (owner == recoveringClient) continue;
        List<Byte> owned = owner.getCpuRacerIndices();
        if (owned.remove(cpuIndex)) {
          owner.setCpuRacerIndices(owned);
          new CpuAssignmentMessage(owner).Send();
          restored.add(cpuIndex);
          break;
        }
      }
    }
    recoveringClient.setCpuRacerIndices(restored);
    recoveringClient.cpuHandoffActive = false;
    new CpuAssignmentMessage(recoveringClient).Send();
  }

  private void DisconnectClientsWithRepeatedRaceData() {
    for (FalconnectClientConnection client : getClients()) {
      if (client.state != ClientState.IN_MENUS && client.duplicateRaceDataDetected &&
          !client.disconnected) {
        System.out.println("Client sent repeated player data during race: " + client.playerNum);
        DisconnectClient(client, "Repeated vehicle data suggests the game has stopped responding.");
      }
    }
  }

  private void DisconnectClient(FalconnectClientConnection client, String reason) {
    try {
      TransferCpuAssignments(client, false);
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
    }

    client.Disconnect(reason);
  }

  private long packetNum = 0;

  public void ConstructFullDataPacket() {
    byte[] packet = new byte[RaceDataFormat.FULL_RACE_PACKET_BYTES];
    packet[0] = (byte) ToClientPacketType.FULL_DATA.ordinal();
    packetNum++;

    //System.out.println(packetNum);

    packet[1] = (byte) ((packetNum >>> 24) & 0xff);
    packet[2] = (byte) ((packetNum >>> 16) & 0xff);
    packet[3] = (byte) ((packetNum >>> 8) & 0xff);
    packet[4] = (byte) ((packetNum) & 0xff);

    //HashMap<Integer, Set<Float>> playerPositions = new HashMap<>();

    for (FalconnectClientConnection client : getClients()) {
      FalconnectClientConnection.ReceivedRaceData receivedRaceData =
          client.getLatestReceivedRaceData();
      int i = 0;
      List<Byte> cpuIndices = receivedRaceData.getCpuRacerIndices();
      List<Byte> ownedCpuIndices = client.getCpuRacerIndices();

      for (byte[] racerData : receivedRaceData.getRacerData()) {
        if (i == 0) {
          // Player's data
          int racerIndex = client.playerNum - 1;
          writeRacerRecord(packet, racerIndex, racerData,
              sourceMetadata(client, receivedRaceData.getSourceSequence()));

          //playerPositions.put(client.playerNum - 1, new HashSet<>());
          // Get racer's position from received data
//          byte[] x = new byte[4];
//          byte[] y = new byte[4];
//          byte[] z = new byte[4];
//          System.arraycopy(racerData, 4, x, 0, 4);
//          System.arraycopy(racerData, 8, y, 0, 4);
//          System.arraycopy(racerData, 12, z, 0, 4);
//
//          playerPositions.get(client.playerNum - 1).add(ByteBuffer.wrap(x).order(ByteOrder.BIG_ENDIAN).getFloat());
//          playerPositions.get(client.playerNum - 1).add(ByteBuffer.wrap(y).order(ByteOrder.BIG_ENDIAN).getFloat());
//          playerPositions.get(client.playerNum - 1).add(ByteBuffer.wrap(z).order(ByteOrder.BIG_ENDIAN).getFloat());

        } else {
          // A packet may have arrived immediately before an assignment update. Its CPU records
          // describe the sender's former ownership, so they must not overwrite the new owner's
          // records in the shared race packet.
          if (i - 1 >= cpuIndices.size() || !ownedCpuIndices.contains(cpuIndices.get(i - 1))) {
            i++;
            continue;
          }

          // CPU data
          int racerIndex = cpuIndices.get(i - 1);
          writeRacerRecord(packet, racerIndex, racerData,
              sourceMetadata(client, receivedRaceData.getSourceSequence()));

          //playerPositions.put(client.cpuStartIndex + i - 1, new HashSet<>());
          // Get racer's position from received data
//          byte[] x = new byte[4];
//          byte[] y = new byte[4];
//          byte[] z = new byte[4];
//          System.arraycopy(racerData, 4, x, 0, 4);
//          System.arraycopy(racerData, 8, y, 0, 4);
//          System.arraycopy(racerData, 12, z, 0, 4);
//
//          playerPositions.get(client.cpuStartIndex + i - 1).add(ByteBuffer.wrap(x).order(ByteOrder.BIG_ENDIAN).getFloat());
//          playerPositions.get(client.cpuStartIndex + i - 1).add(ByteBuffer.wrap(y).order(ByteOrder.BIG_ENDIAN).getFloat());
//          playerPositions.get(client.cpuStartIndex + i - 1).add(ByteBuffer.wrap(z).order(ByteOrder.BIG_ENDIAN).getFloat());
        }

        i++;
      }
    }

    // Calculate offsets for each racer's position
//    for (FalconnectClientConnection client : getClients()) {
//      byte[] toSend = new byte[RaceDataFormat.FULL_RACE_PACKET_BYTES];
//      System.arraycopy(packet, 0, toSend, 0, RaceDataFormat.FULL_RACE_PACKET_BYTES);
//
//      for (Integer racer : playerPositions.keySet()) {
//
//      }
//    }

    fullDataPacket = packet;
    // System.out.println("Packet constructed");
  }

  private static void writeRacerRecord(byte[] packet, int racerIndex, byte[] racerData,
                                       int sourceSequence) {
    int offset = RaceDataFormat.serverRecordOffset(racerIndex);
    System.arraycopy(racerData, 0, packet, offset, RaceDataFormat.RACER_DATA_BYTES);
    int sequenceOffset = offset + RaceDataFormat.RACER_DATA_BYTES;
    packet[sequenceOffset] = (byte) (sourceSequence >>> 24);
    packet[sequenceOffset + 1] = (byte) (sourceSequence >>> 16);
    packet[sequenceOffset + 2] = (byte) (sourceSequence >>> 8);
    packet[sequenceOffset + 3] = (byte) sourceSequence;
  }

  private static int sourceMetadata(FalconnectClientConnection client, int sourceSequence) {
    // Player ownership is part of the metadata so a CPU handoff starts a new sequence domain.
    // The remaining 24 bits are ample for one race and preserve packet ordering per source.
    return ((client.playerNum & 0xff) << 24) |
        (sourceSequence & 0x00ff_ffff);
  }

  public void SendFullDataPacket() throws IOException, InterruptedException {
    ConstructFullDataPacket();

    for (FalconnectClientConnection client : getClients()) {
      //if (client.hasUpdated) {
        FullDataMessage message = new FullDataMessage(client, fullDataPacket);
        message.Send();
        client.hasUpdated = false;
      //} else {
        //System.out.println("Skipped " + client.uid);
      //}
    }

    //System.out.println("Full data packet distributed");
  }
}
