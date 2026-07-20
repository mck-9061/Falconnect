package net.falconnect.messages;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.messages.fromclient.FromClientMessage;
import net.falconnect.messages.fromclient.FullDataMessage;
import net.falconnect.messages.fromclient.UpdateStateMessage;
import net.falconnect.messages.toclient.ToClientMessage;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Queue;

public class MessageHandlerThread extends Thread {
  FalconnectClientConnection clientConnection;

  public Queue<ToClientMessage> messagesToSend;

  // Dictionary of message type bytes to from-client message classes
  HashMap<Byte, Class<? extends FromClientMessage>> messageTypes = new HashMap<>();

  public MessageHandlerThread(FalconnectClientConnection clientConnection) {
    this.clientConnection = clientConnection;

    messagesToSend = new ArrayDeque<>();

    messageTypes.put((byte) 0x0, UpdateStateMessage.class);
    messageTypes.put((byte) 0x1, FullDataMessage.class);
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
          ToClientMessage message = messagesToSend.remove();
          message.SendDataFromThread();
          // System.out.println("Sent message");
        }

      } catch (IOException | InvocationTargetException | InstantiationException |
               IllegalAccessException | NoSuchMethodException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
