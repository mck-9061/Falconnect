package net.falconnect;

import net.falconnect.messages.toclient.*;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class FalconnectRace extends Thread {
  private List<FalconnectClientConnection> clients;
  private MasterServer server;

  private byte[] fullDataPacket;

  public GameState gameState;

  public FalconnectRace(MasterServer server) throws IOException {
    clients = new ArrayList<>();
    gameState = GameState.WAITING_FOR_READY;

    this.server = server;
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
        Thread.sleep(32);
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

          if (allReady) {
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
            for (FalconnectClientConnection client : getClients()) {
              // Re-assign player numbers
              client.playerNum = num;
              num++;

              ConnectedMessage connectedMessage = new ConnectedMessage(client);
              connectedMessage.Send();
            }

            Thread.sleep(1000);

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

  public void ConstructFullDataPacket() {
    byte[] packet = new byte[7680];
    packet[0] = (byte) ToClientPacketType.FULL_DATA.ordinal();
    int cursor = 1;

    for (FalconnectClientConnection client : getClients()) {
      byte[] lastClientData = client.getLastReceivedData();
      client.hasUpdated = false;

      // System.out.println(client.playerNum);

      System.arraycopy(lastClientData, 1, packet, cursor, 255);
      cursor += 255;
    }

    fullDataPacket = packet;
    // System.out.println("Packet constructed");
  }

  public void SendFullDataPacket() throws IOException, InterruptedException {
    ConstructFullDataPacket();

    for (FalconnectClientConnection client : getClients()) {
      FullDataMessage message = new FullDataMessage(client, fullDataPacket);
      message.Send();
    }

    //System.out.println("Full data packet distributed");
  }
}
