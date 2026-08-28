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
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Queue;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class UDPHandlerThread extends Thread {
  FalconnectClientConnection clientConnection;

  public BlockingQueue<ToClientMessage> messagesToProcess;

  // Dictionary of message type bytes to from-client message classes
  HashMap<Byte, Class<? extends FromClientMessage>> messageTypes = new HashMap<>();

  public UDPHandlerThread(FalconnectClientConnection clientConnection) {
    this.clientConnection = clientConnection;

    messagesToProcess = new LinkedBlockingQueue<>();

    messageTypes.put((byte) 0x1, FullDataMessage.class);
  }

  public void run() {
    while (!clientConnection.disconnected) {
      DatagramSocket udpSocket = clientConnection.udpSocket;
      if (udpSocket != null) {
        byte[] data = new byte[RaceDataFormat.packetBytesForRacers(clientConnection.numCpus + 1)];


        DatagramPacket datagramPacket = new DatagramPacket(data, data.length);
        try {
          udpSocket.setSoTimeout(5000);
          udpSocket.receive(datagramPacket);

          clientConnection.lastReceivedUdpPort = datagramPacket.getPort();

          byte messageType = data[0];
          FromClientMessage message = messageTypes.get(messageType).getDeclaredConstructor(FalconnectClientConnection.class, byte[].class).newInstance(clientConnection, data);
          message.ProcessMessage();

        } catch (SocketTimeoutException e) {
          //System.out.println("Timeout");
        } catch (SocketException e) {
          // An intentional close wakes receive() so this long-lived thread can use the next socket.
          if (clientConnection.disconnected) return;
        } catch (IOException | InstantiationException | InvocationTargetException |
                 IllegalAccessException | NoSuchMethodException e) {
          throw new RuntimeException(e);
        }
      } else {
        try {
          Thread.sleep(1000);
        } catch (InterruptedException e) {
          throw new RuntimeException(e);
        }
      }
    }
  }
}
