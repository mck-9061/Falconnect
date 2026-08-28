package net.falconnect.messages;

import net.falconnect.ClientState;
import net.falconnect.FalconnectClientConnection;
import net.falconnect.GameState;
import net.falconnect.Main;
import net.falconnect.RaceDataFormat;
import net.falconnect.messages.fromclient.*;
import net.falconnect.messages.toclient.ToClientMessage;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.io.EOFException;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Queue;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class MessageHandlerThread extends Thread {
  FalconnectClientConnection clientConnection;

  // Dictionary of message type bytes to from-client message classes
  HashMap<Byte, Class<? extends FromClientMessage>> messageTypes = new HashMap<>();

  public MessageHandlerThread(FalconnectClientConnection clientConnection) {
    this.clientConnection = clientConnection;

    messageTypes.put((byte) 0x0, UpdateStateMessage.class);
    messageTypes.put((byte) 0x1, FullDataMessage.class);
    messageTypes.put((byte) 0x2, SettingsMessage.class);
    messageTypes.put((byte) 0x3, ResetMessage.class);
    messageTypes.put((byte) 0x4, DisconnectMessage.class);
    messageTypes.put((byte) 0x5, NameMessage.class);
  }

  public void run() {
    while (true) {
      // TCP carries control messages and must retain one stable frame size across CPU reassignment.
      byte[] data = new byte[RaceDataFormat.FULL_RACE_PACKET_BYTES];

      try {
        clientConnection.fromClientStream.readFully(data);

        byte messageType = data[0];
        FromClientMessage message = messageTypes.get(messageType)
            .getDeclaredConstructor(FalconnectClientConnection.class, byte[].class)
            .newInstance(clientConnection, data);
        message.ProcessMessage();

        if (clientConnection.disconnected) {
          System.out.println("Client disconnected gracefully: " + clientConnection.playerNum);

          clientConnection.socket.close();

          return;
        }

      } catch (EOFException e) {
        System.out.println("Client disconnected ungracefully: " + clientConnection.playerNum);
        clientConnection.disconnected = true;
        clientConnection.CloseUdpSocket();
        return;
      } catch (IOException e) {
        System.out.println("Client connection error: " + clientConnection.playerNum + ": " + e.getMessage());
        clientConnection.disconnected = true;
        clientConnection.CloseUdpSocket();
        return;
      } catch (InvocationTargetException | InstantiationException |
               IllegalAccessException | NoSuchMethodException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
