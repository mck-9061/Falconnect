package net.falconnect;

import java.io.IOException;
import java.net.DatagramSocket;
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
        FalconnectClientConnection clientConnection = new FalconnectClientConnection(clientSocket);
        clientConnection.uid = uid;
        uid++;

        System.out.println("Client connected: " + ((InetSocketAddress) clientSocket.getRemoteSocketAddress()).getAddress());

        server.AllocateClient(clientConnection);

      } catch (IOException | InterruptedException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
