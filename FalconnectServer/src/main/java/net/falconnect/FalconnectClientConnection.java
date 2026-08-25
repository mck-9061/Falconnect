package net.falconnect;

import net.falconnect.messages.MessageHandlerThread;
import net.falconnect.messages.UDPHandlerThread;
import net.falconnect.messages.toclient.ToClientMessage;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

public class FalconnectClientConnection {
  public Socket socket;
  public DatagramSocket udpSocket;
  public DataInputStream fromClientStream;
  private final DataOutputStream toClientStream;
  public ClientState state;

  public int uid;
  public byte playerNum;
  public byte racerId;
  public byte selectedCourse;
  public byte[] name;
  public byte numCpus;
  public byte cpuStartIndex;

  public int lastReceivedUdpPort;
  public int lastReceivedCount;

  public boolean hasUpdated = false;
  public boolean disconnected = false;

  private final MessageHandlerThread receiveMessageThread;
  private final UDPHandlerThread udpHandlerThread;

  private List<byte[]> lastReceivedData;

  public FalconnectClientConnection(Socket socket) throws IOException {
    this.socket = socket;
    lastReceivedData = new ArrayList<>();

    fromClientStream = new DataInputStream(socket.getInputStream());
    toClientStream = new DataOutputStream(socket.getOutputStream());

    state = ClientState.IN_MENUS;
    playerNum = 1;
    racerId = 6;
    selectedCourse = 1;
    name = new byte[32];
    name[0] = 0x46;
    numCpus = 0;
    cpuStartIndex = 1;
    lastReceivedCount = 0;

    receiveMessageThread = new MessageHandlerThread(this);
    receiveMessageThread.start();

    udpHandlerThread = new UDPHandlerThread(this);
    udpHandlerThread.start();
    System.out.println("Message receiver thread started");
  }

  public void SendPacket(byte[] packet) throws IOException, InterruptedException {
    //Thread.sleep(4);

    if (packet[0] == ToClientPacketType.FULL_DATA.ordinal()) {
      if (lastReceivedUdpPort != 0) {
        DatagramPacket dPacket = new DatagramPacket(packet, packet.length, socket.getInetAddress(), lastReceivedUdpPort);
        udpSocket.send(dPacket);
      } else {
        //System.out.println("No port!");
      }

      return;
    }
    toClientStream.write(packet);
  }

  public void SendMessage(ToClientMessage message) throws InterruptedException {
    receiveMessageThread.messagesToSend.put(message);
  }

  public synchronized List<byte[]> getLastReceivedData() {
    return lastReceivedData;
  }

  public synchronized void setLastReceivedData(List<byte[]> data) {
    this.lastReceivedData = data;
  }

  @Override
  public boolean equals(Object o) {
    if (!(o instanceof FalconnectClientConnection other)) return false;

    return other.socket.equals(socket);
  }
}
