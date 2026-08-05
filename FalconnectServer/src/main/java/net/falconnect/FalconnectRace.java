package net.falconnect;

import net.falconnect.messages.toclient.*;

import java.io.IOException;
import java.net.DatagramSocket;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

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
    while (true) {
      try {
        Thread.sleep(4);
      } catch (InterruptedException e) {
        throw new RuntimeException(e);
      }

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

              client.numCpus = (byte) (int) Math.floor((30.0 - getClients().size()) / getClients().size());
              //client.numCpus = 5;
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
              courseMessage.Send();
              RacerIdsMessage racerIdsMessage = new RacerIdsMessage(client, getClients());
              racerIdsMessage.Send();
              NamesMessage namesMessage = new NamesMessage(client, getClients());
              namesMessage.Send();
              StatusMessage message = new StatusMessage(client, ToClientPacketType.START);
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
    byte[] packet = new byte[7680];
    packet[0] = (byte) ToClientPacketType.FULL_DATA.ordinal();
    packetNum++;

    packet[1] = (byte) ((packetNum >>> 24) & 0xff);
    packet[2] = (byte) ((packetNum >>> 16) & 0xff);
    packet[3] = (byte) ((packetNum >>> 8) & 0xff);
    packet[4] = (byte) ((packetNum) & 0xff);

    int cursor;

    for (FalconnectClientConnection client : getClients()) {
      int i = 0;

      for (byte[] racerData : client.getLastReceivedData()) {
        if (i == 0) {
          // Player's data
          cursor = ((client.playerNum - 1) * 255) + 5;
          System.arraycopy(racerData, 0, packet, cursor, 255);
        } else {
          // CPU data
          cursor = ((client.cpuStartIndex + i - 1) * 255) + 5;
          System.arraycopy(racerData, 0, packet, cursor, 255);
        }

        i++;
      }
    }

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
