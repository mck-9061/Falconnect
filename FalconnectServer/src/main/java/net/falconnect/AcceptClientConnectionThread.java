package net.falconnect;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

public class AcceptClientConnectionThread extends Thread {
  FalconnectServer server;

  public AcceptClientConnectionThread(FalconnectServer server) {
    super();
    this.server = server;
  }

  public void run() {
    int uid = 0;
    while (true) {
      System.out.println("Looking for clients...");

      try {
        Socket clientSocket = server.socket.accept();
        int clientUid = uid++;
        // A silent or outdated peer must not block the accept loop or enter a lobby.
        Thread connectionThread = new Thread(() -> acceptClient(clientSocket, clientUid),
            "Falconnect handshake " + clientUid);
        connectionThread.setDaemon(true);
        connectionThread.start();
      } catch (IOException e) {
        if (server.socket.isClosed()) return;
        System.out.println("Unable to accept client: " + e.getMessage());
      }
    }
  }

  private void acceptClient(Socket socket, int uid) {
    FalconnectClientConnection client = null;
    try {
      client = new FalconnectClientConnection(socket);
      client.uid = uid;
      System.out.println("Client connected: "
          + ((InetSocketAddress) socket.getRemoteSocketAddress()).getAddress());
      server.AllocateClient(client);
    } catch (IOException | InterruptedException e) {
      System.out.println("Client connection rejected: " + e.getMessage());
      if (e instanceof InterruptedException) Thread.currentThread().interrupt();
      if (client != null) client.Disconnect("Unable to join a game.");
      try {
        socket.close();
      } catch (IOException closeError) {
        System.out.println("Unable to close rejected connection: " + closeError.getMessage());
      }
    }
  }
}
