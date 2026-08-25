package net.falconnect.messages;

import net.falconnect.ClientState;
import net.falconnect.FalconnectClientConnection;
import net.falconnect.GameState;
import net.falconnect.Main;
import net.falconnect.messages.fromclient.*;
import net.falconnect.messages.toclient.FullDataMessage;
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

public class MessageSenderThread extends Thread {
  FalconnectClientConnection clientConnection;

  public BlockingQueue<ToClientMessage> messagesToSend;

  public MessageSenderThread(FalconnectClientConnection clientConnection) {
    this.clientConnection = clientConnection;

    messagesToSend = new LinkedBlockingQueue<>();
  }

  public void run() {
    while (!clientConnection.disconnected) {
      try {
        // Check if there's any messages to send
        if (!messagesToSend.isEmpty()) {
          ToClientMessage message = messagesToSend.take();

          if (message instanceof FullDataMessage) {
            while (!messagesToSend.isEmpty()) {
              ToClientMessage nMessage = messagesToSend.take();
              if (nMessage instanceof FullDataMessage) {
                message = nMessage;
                System.out.println("Skipped old message");
              } else {
                message = nMessage;
                break;
              }
            }
          }

          message.SendDataFromThread();
        }

      } catch (IOException | InterruptedException e) {
        throw new RuntimeException(e);
      }
    }
  }
}
