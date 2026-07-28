package net.falconnect;

import net.falconnect.messages.toclient.ConnectedMessage;
import net.falconnect.messages.toclient.StatusMessage;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

public class AcceptClientConnectionThread extends Thread {
  FalconnectServer server;
  public byte num = 1;
  public volatile boolean wait = false;

  public AcceptClientConnectionThread(FalconnectServer server) {
    super();
    this.server = server;
  }

  public void run() {
    while (true) {
      if (server.getClients().size() < 30) {
        System.out.println("Looking for clients...");

        try {
          Socket clientSocket = server.socket.accept();
          FalconnectClientConnection clientConnection = new FalconnectClientConnection(clientSocket);

          while (wait) {
            Thread.onSpinWait();
          }

          wait = true;

          server.AddClient(clientConnection);

          clientConnection.playerNum = num;
          num++;

          System.out.println("Client connected: " + ((InetSocketAddress) clientSocket.getRemoteSocketAddress()).getAddress());

          ConnectedMessage message = new ConnectedMessage(clientConnection);
          message.Send();

          wait = false;

        } catch (IOException | InterruptedException e) {
          throw new RuntimeException(e);
        }

      } else {
        System.out.println("Server full!");
        return;
      }
    }
  }
}
