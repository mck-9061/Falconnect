package net.falconnect;

import net.falconnect.messages.MessageHandlerThread;
import net.falconnect.messages.toclient.ToClientMessage;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;

public class FalconnectClientConnection {
  public Socket socket;
  public DataInputStream fromClientStream;
  private final DataOutputStream toClientStream;
  public ClientState state;

  public byte playerNum;
  public byte racerId;
  public byte selectedCourse;

  public boolean hasUpdated = false;
  public boolean disconnected = false;

  private final MessageHandlerThread receiveMessageThread;

  private byte[] lastReceivedData;

  public FalconnectClientConnection(Socket socket) throws IOException {
    this.socket = socket;
    lastReceivedData = new byte[256];

    fromClientStream = new DataInputStream(socket.getInputStream());
    toClientStream = new DataOutputStream(socket.getOutputStream());

    state = ClientState.IN_MENUS;
    racerId = 6;
    selectedCourse = 1;

    receiveMessageThread = new MessageHandlerThread(this);
    receiveMessageThread.start();
    System.out.println("Message receiver thread started");
  }

  public void SendPacket(byte[] packet) throws IOException {
    toClientStream.write(packet);
  }

  public void SendMessage(ToClientMessage message) throws InterruptedException {
    receiveMessageThread.messagesToSend.put(message);
  }

  public synchronized byte[] getLastReceivedData() {
    return lastReceivedData;
  }

  public synchronized void setLastReceivedData(byte[] data) {
    this.lastReceivedData = data;
  }

  @Override
  public boolean equals(Object o) {
    if (!(o instanceof FalconnectClientConnection other)) return false;

    return other.playerNum == playerNum;
  }
}
