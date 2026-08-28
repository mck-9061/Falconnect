package net.falconnect;

import net.falconnect.messages.toclient.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.*;

public class FalconnectRace extends Thread {
  private static final long UDP_TIMEOUT_MILLIS = 5_000;
  private static final long CPU_HANDOFF_TIMEOUT_MILLIS = 750;
  private List<FalconnectClientConnection> clients;
  private FalconnectServer server;
  private final int portBlockIndex;

  private byte[] fullDataPacket;

  public GameState gameState;
  public boolean isRunning;

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
    return clients.size() < FalconnectServer.MAX_CLIENTS_PER_RACE;
  }

  public int getPortBlockIndex() {
    return portBlockIndex;
  }

  public int getUdpPort(byte playerNum) {
    return FalconnectServer.UDP_PORT_BASE
        - (portBlockIndex * FalconnectServer.MAX_CLIENTS_PER_RACE)
        - playerNum;
  }

  public synchronized boolean IsClientsEmpty() {
    return clients.isEmpty();
  }

  public synchronized List<FalconnectClientConnection> getClients() {
    return clients;
  }

  public synchronized void AddClient(FalconnectClientConnection client) {
    client.udpPort = getUdpPort(client.playerNum);
    clients.add(client);
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
    while (true) {
      // wait for a frame to pass
      long currentTime = System.nanoTime();
      if (currentTime - time < 16666666) continue;

      time = currentTime;


      try {
        // Wait for all players to be ready
        // System.out.println("???");

        if (IsClientsEmpty()) return;

        // System.out.println(gameState);

        if (gameState == GameState.WAITING_FOR_READY) {
          RemoveDisconnectedClients();
          boolean allReady = true;
          for (FalconnectClientConnection client : getClients()) {
            // System.out.println(client.playerNum + " " + client.state);
            if (client.state != ClientState.READY) {
              allReady = false;
              break;
            }
          }

          if (allReady && getClients().size() > 1) {
            System.out.println("Starting!");
            gameState = GameState.WAITING_FOR_GRID;

            if (IsClientsEmpty()) {
              System.out.println("Nevermind all the players left");

              RemoveDisconnectedClients();

              server.EndRace(this);
              return;
            }

            // Select a course from those voted for
            List<Byte> courses = new ArrayList<>();
            for (FalconnectClientConnection client : getClients())
              courses.add(client.selectedCourse);
            byte usedCourse = courses.get((new Random()).nextInt(courses.size()));

            byte num = 1;

            RemoveDisconnectedClients();
            int nextCpuIndex = getClients().size();
            int totalCpus = RaceDataFormat.MAX_RACERS - getClients().size();
            int baseCpuCount = totalCpus / getClients().size();
            int remainder = totalCpus % getClients().size();
            for (FalconnectClientConnection client : getClients()) {
              // Re-assign player numbers
              client.playerNum = num;
              num++;

              //client.numCpus = (byte) (int) Math.floor((30.0 - getClients().size()) / getClients().size());
              //client.numCpus = 5;

              int cpuCount = baseCpuCount + (client.playerNum <= remainder ? 1 : 0);
              List<Byte> cpuIndices = new ArrayList<>();
              for (int i = 0; i < cpuCount; i++) cpuIndices.add((byte) nextCpuIndex++);
              client.setHomeCpuRacerIndices(cpuIndices);
              client.cpuStartIndex = cpuIndices.isEmpty() ? 0 : cpuIndices.get(0);
              client.udpPort = getUdpPort(client.playerNum);

              client.RebindUdpSocket();

              ConnectedMessage connectedMessage = new ConnectedMessage(client);
              connectedMessage.Send();
            }

            Thread.sleep(1000);

            RacerIdsMessage.shouldRandomise = true;

            for (FalconnectClientConnection client : getClients()) {
              new CpuAssignmentMessage(client).Send();
              CourseMessage courseMessage = new CourseMessage(client, usedCourse, (byte) (getClients().size() - 1));
              Thread.sleep(20);
              courseMessage.Send();
              RacerIdsMessage racerIdsMessage = new RacerIdsMessage(client, getClients());
              Thread.sleep(20);
              racerIdsMessage.Send();
              NamesMessage namesMessage = new NamesMessage(client, getClients());
              Thread.sleep(20);
              namesMessage.Send();
              StatusMessage message = new StatusMessage(client, ToClientPacketType.START);
              Thread.sleep(20);
              message.Send();

              client.lastReceivedCount = 0;
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
            gameState = GameState.WAITING_FOR_READY;

            RemoveDisconnectedClients();

            server.EndRace(this);
            isRunning = false;
            return;
          }
        }
      } catch (InterruptedException | IOException e) {
        throw new RuntimeException(e);
      }
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
    FalconnectClientConnection recipient = getClients().stream()
        // TCP state changes can lag behind the race transition. Only give simulation work to a
        // client which is demonstrably still participating by delivering fresh UDP race data.
        .filter(other -> other != source && !other.disconnected && other.hasReceivedRaceData &&
            now - other.lastUdpPacketReceivedAt <= CPU_HANDOFF_TIMEOUT_MILLIS)
        .min(Comparator.comparingInt(other -> other.getCpuRacerIndices().size()))
        .orElse(null);
    if (recipient == null) {
      System.out.println("No active client is available to receive CPUs from player " + source.playerNum);
      return;
    }

    List<Byte> sourceCpuIndices = source.getCpuRacerIndices();
    if (sourceCpuIndices.isEmpty()) return;

    List<Byte> recipientCpuIndices = recipient.getCpuRacerIndices();
    recipientCpuIndices.addAll(sourceCpuIndices);
    sourceCpuIndices.clear();
    source.setCpuRacerIndices(sourceCpuIndices);
    recipient.setCpuRacerIndices(recipientCpuIndices);
    source.cpuHandoffActive = notifySource;

    System.out.println("Transferred CPUs " + recipientCpuIndices + " to player " + recipient.playerNum);
    try {
      // These are state-changing control messages, not lossy race data. Send them immediately so
      // the recipient begins producing frames before the next server broadcast.
      if (notifySource) new CpuAssignmentMessage(source).SendDataFromThread();
      new CpuAssignmentMessage(recipient).SendDataFromThread();
    } catch (IOException e) {
      System.out.println("Unable to deliver CPU assignment to player " + recipient.playerNum);
      recipient.disconnected = true;
      recipient.CloseUdpSocket();
    }
  }

  private void RestoreHomeCpuAssignment(FalconnectClientConnection recoveringClient) throws InterruptedException {
    List<Byte> restored = recoveringClient.getCpuRacerIndices();
    for (Byte cpuIndex : recoveringClient.getHomeCpuRacerIndices()) {
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

    try {
      new DisconnectMessage(client, reason).SendDataFromThread();
    } catch (IOException | InterruptedException e) {
      System.out.println("Unable to send disconnect notice to client " + client.playerNum);
    }
    client.disconnected = true;
    client.CloseUdpSocket();
    try {
      client.socket.close();
    } catch (IOException e) {
      System.out.println("Unable to close TCP connection for player " + client.playerNum);
    }
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

    int cursor;

    for (FalconnectClientConnection client : getClients()) {
      int i = 0;
      List<Byte> cpuIndices = client.getLastReceivedCpuRacerIndices();
      List<Byte> ownedCpuIndices = client.getCpuRacerIndices();

      for (byte[] racerData : client.getLastReceivedData()) {
        if (i == 0) {
          // Player's data
          cursor =
              ((client.playerNum - 1) * RaceDataFormat.RACER_DATA_BYTES)
                  + RaceDataFormat.PACKET_HEADER_BYTES;
          System.arraycopy(racerData, 0, packet, cursor, RaceDataFormat.RACER_DATA_BYTES);

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
          cursor =
              (cpuIndices.get(i - 1) * RaceDataFormat.RACER_DATA_BYTES)
                  + RaceDataFormat.PACKET_HEADER_BYTES;
          System.arraycopy(racerData, 0, packet, cursor, RaceDataFormat.RACER_DATA_BYTES);

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
