package net.falconnect;

import net.falconnect.messages.toclient.*;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class FalconnectServer {
  public ServerSocket socket;
  private List<FalconnectClientConnection> clients;

  public AcceptClientConnectionThread acceptClientConnectionThread;
  private byte[] fullDataPacket;

  public GameState gameState;

  public FalconnectServer() throws IOException {
    clients = new ArrayList<>();
    gameState = GameState.WAITING_FOR_READY;

    socket = new ServerSocket(8000);
    System.out.println("Server socket created");

    acceptClientConnectionThread = new AcceptClientConnectionThread(this);
    acceptClientConnectionThread.start();
    System.out.println("Client connection acceptance thread started");
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

  public void MainLoop() throws IOException, InterruptedException {
    // Wait for all players to be ready
    // System.out.println("???");

    if (IsClientsEmpty()) return;

    // System.out.println(gameState);

    if (gameState == GameState.WAITING_FOR_READY) {
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

        // Select a course from those voted for
        List<Byte> courses = new ArrayList<>();
        for (FalconnectClientConnection client : getClients()) courses.add(client.selectedCourse);
        byte usedCourse = courses.get((new Random()).nextInt(courses.size()));

        while (acceptClientConnectionThread.wait) {}
        acceptClientConnectionThread.wait = true;
        acceptClientConnectionThread.num = 1;

        for (FalconnectClientConnection client : getClients()) {
          // Re-assign player numbers
          client.playerNum = acceptClientConnectionThread.num;
          acceptClientConnectionThread.num++;

          ConnectedMessage connectedMessage = new ConnectedMessage(client);
          connectedMessage.Send();
        }

        acceptClientConnectionThread.wait = false;

        Thread.sleep(1000);

        for (FalconnectClientConnection client : getClients()) {
          CourseMessage courseMessage = new CourseMessage(client, usedCourse, (byte) (getClients().size() - 1));
          courseMessage.Send();
          RacerIdsMessage racerIdsMessage = new RacerIdsMessage(client, getClients());
          racerIdsMessage.Send();
          StatusMessage message = new StatusMessage(client, ToClientPacketType.START);
          message.Send();
        }
      }
    }

    // Wait for all players to be gridded
    if (gameState == GameState.WAITING_FOR_GRID) {
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
//      boolean allUpdated = true;
//      for (FalconnectClientConnection client : getClients()) {
//        if (!client.hasUpdated) {
//          allUpdated = false;
//          break;
//        }
//      }
//
//      if (allUpdated) {
        SendFullDataPacket();
      //}

      // If all players finished, reset game state
      boolean allDone = true;
      for (FalconnectClientConnection client : getClients()) {
        if (client.state == ClientState.RACING ||
        client.state == ClientState.GRIDDED) {
          allDone = false;
          break;
        }
      }

      if (allDone) {
        System.out.println("Resetting!");
        gameState = GameState.WAITING_FOR_READY;

        // Remove disconnected clients
        List<FalconnectClientConnection> disconnectedClients = new ArrayList<>();
        for (FalconnectClientConnection client : getClients()) {
          if (client.disconnected) disconnectedClients.add(client);
        }

        for (FalconnectClientConnection client : disconnectedClients) {
          RemoveClient(client);
        }

        while (acceptClientConnectionThread.wait) {}
        acceptClientConnectionThread.wait = true;
        acceptClientConnectionThread.num = 1;
        for (FalconnectClientConnection client : getClients()) {
          // Re-assign player numbers
          client.playerNum = acceptClientConnectionThread.num;
          acceptClientConnectionThread.num++;

          ConnectedMessage connectedMessage = new ConnectedMessage(client);
          connectedMessage.Send();
        }

        acceptClientConnectionThread.wait = false;
      }
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
