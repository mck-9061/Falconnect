package net.falconnect;

import net.falconnect.messages.toclient.*;

import java.io.IOException;
import java.net.DatagramSocket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.*;

public class FalconnectRace extends Thread {
  private List<FalconnectClientConnection> clients;
  private FalconnectServer server;

  private byte[] fullDataPacket;

  public GameState gameState;
  public boolean isRunning;

  public FalconnectRace(FalconnectServer server) throws IOException {
    clients = new ArrayList<>();
    gameState = GameState.WAITING_FOR_READY;

    this.server = server;
    isRunning = true;

    //HackyAssSoftlockPreventionThread hackyAssSoftlockPreventionThread = new HackyAssSoftlockPreventionThread(this);
    //hackyAssSoftlockPreventionThread.start();
  }

  public synchronized boolean HasSpace() {
    return clients.size() < 6;
  }

  public synchronized boolean IsClientsEmpty() {
    return clients.isEmpty();
  }

  public synchronized List<FalconnectClientConnection> getClients() {
    return clients;
  }

  public synchronized void AddClient(FalconnectClientConnection client) {
    clients.add(client);
  }

  public synchronized void RemoveClient(FalconnectClientConnection client) {
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
            int aaa = 0;
            for (FalconnectClientConnection client : getClients()) {
              // Re-assign player numbers
              client.playerNum = num;
              num++;

              //client.numCpus = (byte) (int) Math.floor((30.0 - getClients().size()) / getClients().size());
              //client.numCpus = 5;

              if (client.playerNum == 2) client.numCpus = 28;
              else client.numCpus = 0;

              client.cpuStartIndex = (byte) (getClients().size() + aaa);
              aaa += client.numCpus;

              if (client.udpSocket != null) client.udpSocket.close();

              client.udpSocket = new DatagramSocket(9000 - client.playerNum); // listens

              ConnectedMessage connectedMessage = new ConnectedMessage(client);
              connectedMessage.Send();
            }

            Thread.sleep(1000);

            RacerIdsMessage.shouldRandomise = true;

            for (FalconnectClientConnection client : getClients()) {
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
            if (client.state != ClientState.GRIDDED && client.state != ClientState.IN_MENUS) {
              allGridded = false;
              break;
            }
          }

          if (allGridded) {
            gameState = GameState.RACING;
            for (FalconnectClientConnection client : getClients()) {
              StatusMessage message = new StatusMessage(client, ToClientPacketType.START_RACE);
              message.Send();
            }
          }
        }

        // Send machine data
        if (gameState == GameState.RACING) {
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

  public void RemoveDisconnectedClients() {
    // Remove disconnected clients
    List<FalconnectClientConnection> disconnectedClients = new ArrayList<>();
    for (FalconnectClientConnection client : getClients()) {
      if (client.disconnected) disconnectedClients.add(client);
    }

    for (FalconnectClientConnection client : disconnectedClients) {
      RemoveClient(client);
    }
  }

  private long packetNum = 0;

  public void ConstructFullDataPacket() {
    byte[] packet = new byte[3844];
    packet[0] = (byte) ToClientPacketType.FULL_DATA.ordinal();
    packetNum++;

    System.out.println(packetNum);

    packet[1] = (byte) ((packetNum >>> 24) & 0xff);
    packet[2] = (byte) ((packetNum >>> 16) & 0xff);
    packet[3] = (byte) ((packetNum >>> 8) & 0xff);
    packet[4] = (byte) ((packetNum) & 0xff);

    //HashMap<Integer, Set<Float>> playerPositions = new HashMap<>();

    int cursor;

    for (FalconnectClientConnection client : getClients()) {
      int i = 0;

      for (byte[] racerData : client.getLastReceivedData()) {
        if (i == 0) {
          // Player's data
          cursor = ((client.playerNum - 1) * 124) + 5;
          System.arraycopy(racerData, 0, packet, cursor, 124);

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
          // CPU data
          cursor = ((client.cpuStartIndex + i - 1) * 124) + 5;
          System.arraycopy(racerData, 0, packet, cursor, 124);

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
//      byte[] toSend = new byte[3844];
//      System.arraycopy(packet, 0, toSend, 0, 3844);
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
