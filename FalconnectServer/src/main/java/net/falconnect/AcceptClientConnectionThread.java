package net.falconnect;

import net.falconnect.messages.toclient.ConnectedMessage;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

public class AcceptClientConnectionThread extends Thread {
  MasterServer server;

  public AcceptClientConnectionThread(MasterServer server) {
    super();
    this.server = server;
  }

  public void run() {
    while (true) {
      System.out.println("Looking for clients...");

      try {
        Socket clientSocket = server.socket.accept();
        FalconnectClientConnection clientConnection = new FalconnectClientConnection(clientSocket);

        System.out.println("Client connected: " + ((InetSocketAddress) clientSocket.getRemoteSocketAddress()).getAddress());

        server.AllocateClient(clientConnection);

      } catch (IOException | InterruptedException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
