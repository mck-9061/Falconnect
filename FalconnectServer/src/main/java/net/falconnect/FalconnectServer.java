package net.falconnect;

import net.falconnect.messages.toclient.FullDataMessage;
import net.falconnect.messages.toclient.StatusMessage;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.List;

public class FalconnectServer {
  public ServerSocket socket;
  private List<FalconnectClientConnection> clients;

  private AcceptClientConnectionThread acceptClientConnectionThread;
  private byte[] fullDataPacket;

  private GameState gameState;

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

  public void MainLoop() throws IOException {
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
        for (FalconnectClientConnection client : getClients()) {
          StatusMessage message = new StatusMessage(client, ToClientPacketType.START);
          message.Send();
        }
      }
    }

    // Wait for all players to be gridded
    if (gameState == GameState.WAITING_FOR_GRID) {
      boolean allGridded = true;
      for (FalconnectClientConnection client : getClients()) {
        if (client.state != ClientState.GRIDDED) {
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
    }
  }

  public void ConstructFullDataPacket() {
    byte[] packet = new byte[7680];
    packet[0] = (byte) ToClientPacketType.FULL_DATA.ordinal();
    int cursor = 1;

    for (FalconnectClientConnection client : getClients()) {
      byte[] lastClientData = client.getLastReceivedData();
      client.hasUpdated = false;

      System.arraycopy(lastClientData, 1, packet, cursor, 255);
      cursor += 255;
    }

    fullDataPacket = packet;
    // System.out.println("Packet constructed");
  }

  public void SendFullDataPacket() throws IOException {
    ConstructFullDataPacket();

    for (FalconnectClientConnection client : getClients()) {
      FullDataMessage message = new FullDataMessage(client, fullDataPacket);
      message.Send();
    }

    System.out.println("Full data packet distributed");
  }
}
