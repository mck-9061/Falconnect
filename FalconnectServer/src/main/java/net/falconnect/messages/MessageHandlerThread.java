package net.falconnect.messages;

import net.falconnect.ClientState;
import net.falconnect.FalconnectClientConnection;
import net.falconnect.GameState;
import net.falconnect.Main;
import net.falconnect.messages.fromclient.*;
import net.falconnect.messages.toclient.ToClientMessage;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.net.SocketException;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Queue;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class MessageHandlerThread extends Thread {
  FalconnectClientConnection clientConnection;

  public BlockingQueue<ToClientMessage> messagesToSend;

  // Dictionary of message type bytes to from-client message classes
  HashMap<Byte, Class<? extends FromClientMessage>> messageTypes = new HashMap<>();

  public MessageHandlerThread(FalconnectClientConnection clientConnection) {
    this.clientConnection = clientConnection;

    messagesToSend = new LinkedBlockingQueue<>();

    messageTypes.put((byte) 0x0, UpdateStateMessage.class);
    messageTypes.put((byte) 0x1, FullDataMessage.class);
    messageTypes.put((byte) 0x2, SettingsMessage.class);
    messageTypes.put((byte) 0x3, ResetMessage.class);
    messageTypes.put((byte) 0x4, DisconnectMessage.class);
  }

  public void run() {
    while (true) {
      byte[] data = new byte[256];

      try {
        if (clientConnection.fromClientStream.available() > 0) {
          // System.out.println("Receiving message...");
          clientConnection.fromClientStream.read(data);
          // System.out.println("Message received");

          byte messageType = data[0];
          FromClientMessage message = messageTypes.get(messageType).getDeclaredConstructor(FalconnectClientConnection.class, byte[].class).newInstance(clientConnection, data);
          message.ProcessMessage();
        }

        // Check if there's any messages to send
        if (!messagesToSend.isEmpty()) {
          ToClientMessage message = messagesToSend.take();
          message.SendDataFromThread();
          // System.out.println("Sent message");
        }

        if (clientConnection.disconnected) {
          System.out.println("Client disconnected: " + clientConnection.playerNum);

          if (Main.server.gameState != GameState.RACING) {
            Main.server.RemoveClient(clientConnection);
          }

          clientConnection.socket.close();

          return;
        }

      } catch (SocketException e) {
        System.out.println("Client disconnected: " + clientConnection.playerNum);

        if (Main.server.gameState != GameState.RACING) {
          Main.server.RemoveClient(clientConnection);
        }

        try {
          clientConnection.socket.close();
        } catch (IOException ex) {
          throw new RuntimeException(ex);
        }
        return;
      }
      catch (IOException | InvocationTargetException | InstantiationException |
               IllegalAccessException | NoSuchMethodException | InterruptedException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
