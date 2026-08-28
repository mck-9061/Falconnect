package net.falconnect;

import net.falconnect.messages.toclient.ConnectedMessage;
import net.falconnect.messages.toclient.DisconnectMessage;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.List;

public class FalconnectServer {
  public static final int MAX_CLIENTS_PER_RACE = 6;
  // Raising this consumes another MAX_CLIENTS_PER_RACE ports below UDP_PORT_BASE per race.
  public static final int MAX_CONCURRENT_RACES = 3;
  public static final int UDP_PORT_BASE = 9000;

  public ServerSocket socket;
  public List<FalconnectRace> currentRaces;
  private AcceptClientConnectionThread acceptClientConnectionThread;
  private ConsoleCommandHandlerThread consoleCommandHandlerThread;

  public FalconnectServer() throws IOException {
    super();

    currentRaces = new ArrayList<>();

    socket = new ServerSocket(8000);
    System.out.println("Server socket created");

    acceptClientConnectionThread = new AcceptClientConnectionThread(this);
    acceptClientConnectionThread.start();

    consoleCommandHandlerThread = new ConsoleCommandHandlerThread(this);
    consoleCommandHandlerThread.start();
  }

  public synchronized void EndRace(FalconnectRace race) throws IOException, InterruptedException {
    System.out.println("Race ended, re-allocating clients");
    List<FalconnectClientConnection> clients = new ArrayList<>(race.getClients());
    currentRaces.remove(race);

    for (FalconnectClientConnection client : clients) {
      client.CloseUdpSocket();
      AllocateClient(client);
    }
  }

  public synchronized void AllocateClient(FalconnectClientConnection client) throws IOException, InterruptedException {
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
      if (race.HasSpace() && race.gameState == GameState.WAITING_FOR_READY) {
        race.AddClient(client);
        System.out.println("Allocated to existing race");
        SendConnectedMessage(client);
        return;
      }
    }

    if (currentRaces.size() < MAX_CONCURRENT_RACES) {
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
    DisconnectMessage disconnectMessage = new DisconnectMessage(client, "This server is full. Please try again later.");
    disconnectMessage.Send();
    Thread.sleep(100);
    client.socket.close();
  }

  private FalconnectRace CreateRace() throws IOException {
    for (int portBlockIndex = 0; portBlockIndex < MAX_CONCURRENT_RACES; portBlockIndex++) {
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
