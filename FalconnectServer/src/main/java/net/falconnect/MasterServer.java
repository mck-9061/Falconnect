package net.falconnect;

import net.falconnect.messages.toclient.ConnectedMessage;
import net.falconnect.messages.toclient.DisconnectMessage;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.List;

public class MasterServer {
  public ServerSocket socket;
  public List<FalconnectRace> currentRaces;
  private AcceptClientConnectionThread acceptClientConnectionThread;

  public MasterServer() throws IOException {
    super();

    currentRaces = new ArrayList<>();

    socket = new ServerSocket(8000);
    System.out.println("Server socket created");

    acceptClientConnectionThread = new AcceptClientConnectionThread(this);
    acceptClientConnectionThread.start();
  }

  public synchronized void EndRace(FalconnectRace race) throws IOException, InterruptedException {
    System.out.println("Race ended, re-allocating clients");
    List<FalconnectClientConnection> clients = race.getClients();
    currentRaces.remove(race);

    for (FalconnectClientConnection client : clients) {
      AllocateClient(client);
    }
  }

  public synchronized void AllocateClient(FalconnectClientConnection client) throws IOException, InterruptedException {
    ConnectedMessage message = new ConnectedMessage(client);

    System.out.println("Allocating " + client.socket.getRemoteSocketAddress() + "...");

    if (currentRaces.isEmpty()) {
    //if (false) {
      // Start a new race
      FalconnectRace race = new FalconnectRace(this);
      currentRaces.add(race);
      race.AddClient(client);
      race.start();
      System.out.println("Allocated to new race (no current races)");
      message.Send();
      return;
    }

    for (FalconnectRace race : currentRaces) {
      if (race.HasSpace() && race.gameState == GameState.WAITING_FOR_READY) {
        race.AddClient(client);
        System.out.println("Allocated to existing race");
        message.Send();
        return;
      }
    }

    if (currentRaces.size() < 3) {
      // Start a new race
      FalconnectRace race = new FalconnectRace(this);
      currentRaces.add(race);
      race.AddClient(client);
      race.start();
      System.out.println("Allocated to new race");
      message.Send();
      return;
    }

    System.out.println("No space!");

    // No space anywhere for client :(
    DisconnectMessage disconnectMessage = new DisconnectMessage(client, "Server is full!");
    disconnectMessage.Send();
    Thread.sleep(100);
    client.socket.close();
  }
}
