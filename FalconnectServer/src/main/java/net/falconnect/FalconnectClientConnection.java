package net.falconnect;

import net.falconnect.messages.MessageHandlerThread;
import net.falconnect.messages.MessageSenderThread;
import net.falconnect.messages.UDPHandlerThread;
import net.falconnect.messages.toclient.ToClientMessage;
import net.falconnect.messages.toclient.DisconnectMessage;

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
  public volatile DatagramSocket udpSocket;
  public DataInputStream fromClientStream;
  private final DataOutputStream toClientStream;
  public volatile ClientState state;

  public int uid;
  public byte playerNum;
  public byte racerId;
  public byte selectedCourse;
  public byte[] customMachineData;
  public byte[] name;
  public byte numCpus;
  public byte cpuStartIndex;
  public int udpPort;

  public volatile int lastReceivedUdpPort;
  public int lastReceivedCount;
  public volatile long lastUdpPacketReceivedAt;
  public volatile boolean cpuReallocationNeeded;
  public volatile boolean cpuRestorationNeeded;
  public volatile boolean hasReceivedRaceData;
  public volatile boolean cpuHandoffActive;
  public volatile boolean duplicateRaceDataDetected;
  private List<Byte> cpuRacerIndices = new ArrayList<>();
  private List<Byte> previousCpuRacerIndices = new ArrayList<>();
  private List<Byte> homeCpuRacerIndices = new ArrayList<>();
  private byte[] lastPlayerRacerData;
  private int consecutiveDuplicatePlayerDataPackets;

  public boolean hasUpdated = false;
  public volatile boolean disconnected = false;

  private final MessageHandlerThread receiveMessageThread;
  private final MessageSenderThread sendMessageThread;
  private final UDPHandlerThread udpHandlerThread;

  private ReceivedRaceData latestReceivedRaceData = ReceivedRaceData.empty();
  public byte[] dataToSend;

  public FalconnectClientConnection(Socket socket) throws IOException {
    this(socket, true);
  }

  FalconnectClientConnection(Socket socket, boolean startThreads) throws IOException {
    this.socket = socket;
    fromClientStream = new DataInputStream(socket.getInputStream());
    toClientStream = new DataOutputStream(socket.getOutputStream());

    state = ClientState.IN_MENUS;
    playerNum = 1;
    racerId = 6;
    selectedCourse = 1;
    customMachineData = new byte[3];
    name = new byte[32];
    name[0] = 0x46;
    numCpus = 0;
    cpuStartIndex = 1;
    lastReceivedCount = 0;

    receiveMessageThread = new MessageHandlerThread(this);
    sendMessageThread = new MessageSenderThread(this);
    udpHandlerThread = new UDPHandlerThread(this);
    if (startThreads) {
      receiveMessageThread.start();
      sendMessageThread.start();
      udpHandlerThread.start();
      System.out.println("Message receiver thread started");
    }
  }

  public void SendPacket(byte[] packet) throws IOException, InterruptedException {
    if (disconnected) return;
    //Thread.sleep(4);

    if (packet[0] == ToClientPacketType.FULL_DATA.ordinal()) {
      DatagramSocket currentUdpSocket = udpSocket;
      if (lastReceivedUdpPort != 0 && currentUdpSocket != null && !currentUdpSocket.isClosed()) {
        DatagramPacket dPacket = new DatagramPacket(packet, packet.length, socket.getInetAddress(), lastReceivedUdpPort);
        currentUdpSocket.send(dPacket);
      } else {
        //System.out.println("No port!");
      }

      return;
    }
    synchronized (toClientStream) {
      if (!disconnected) toClientStream.write(packet);
    }
  }

  public void SendMessage(ToClientMessage message) throws InterruptedException {
    if (!disconnected) sendMessageThread.messagesToSend.put(message);
  }

  public void Disconnect(String reason) {
    synchronized (toClientStream) {
      try {
        // Deliver the notice before stopping the queued sender. Serialize it with TCP frames.
        if (!disconnected) new DisconnectMessage(this, reason).SendDataFromThread();
      } catch (IOException e) {
        System.out.println("Unable to send disconnect notice to player " + uid);
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
      } finally {
        disconnected = true;
      }
    }
    CloseUdpSocket();
    try {
      socket.close();
    } catch (IOException e) {
      System.out.println("Unable to close TCP connection for player " + uid);
    }
  }

  public synchronized void RebindUdpSocket() throws IOException {
    CloseUdpSocket();
    if (disconnected) return;
    udpSocket = new DatagramSocket(udpPort);
  }

  public synchronized void CloseUdpSocket() {
    DatagramSocket currentUdpSocket = udpSocket;
    udpSocket = null;
    lastReceivedUdpPort = 0;

    if (currentUdpSocket != null) currentUdpSocket.close();
  }

  public static final class ReceivedRaceData {
    private final List<byte[]> racerData;
    private final List<Byte> cpuRacerIndices;
    private final int sourceSequence;

    private ReceivedRaceData(List<byte[]> racerData, List<Byte> cpuRacerIndices,
                             int sourceSequence) {
      this.racerData = racerData;
      this.cpuRacerIndices = cpuRacerIndices;
      this.sourceSequence = sourceSequence;
    }

    public static ReceivedRaceData empty() {
      return new ReceivedRaceData(List.of(), List.of(), 0);
    }

    public List<byte[]> getRacerData() {
      return racerData;
    }

    public List<Byte> getCpuRacerIndices() {
      return cpuRacerIndices;
    }

    public int getSourceSequence() {
      return sourceSequence;
    }
  }

  public synchronized void publishReceivedRaceData(List<byte[]> racerData,
                                                    List<Byte> cpuRacerIndices,
                                                    int sourceSequence) {
    latestReceivedRaceData = new ReceivedRaceData(List.copyOf(racerData),
        List.copyOf(cpuRacerIndices), sourceSequence);
  }

  public synchronized ReceivedRaceData getLatestReceivedRaceData() {
    return latestReceivedRaceData;
  }

  public synchronized List<Byte> getCpuRacerIndices() { return new ArrayList<>(cpuRacerIndices); }
  public synchronized List<Byte> getPreviousCpuRacerIndices() { return new ArrayList<>(previousCpuRacerIndices); }
  public synchronized List<Byte> getHomeCpuRacerIndices() { return new ArrayList<>(homeCpuRacerIndices); }

  public synchronized void setCpuRacerIndices(List<Byte> indices) {
    previousCpuRacerIndices = cpuRacerIndices;
    cpuRacerIndices = new ArrayList<>(indices);
    numCpus = (byte) indices.size();
  }

  public synchronized void setHomeCpuRacerIndices(List<Byte> indices) {
    homeCpuRacerIndices = new ArrayList<>(indices);
    setCpuRacerIndices(indices);
  }

  public boolean usesCustomMachine() {
    for (byte component : customMachineData) {
      if (component != 0) return true;
    }
    return false;
  }

  public synchronized void recordPlayerRacerData(byte[] racerData, int duplicateLimit) {
    if (java.util.Arrays.equals(lastPlayerRacerData, racerData)) {
      consecutiveDuplicatePlayerDataPackets++;
    } else {
      consecutiveDuplicatePlayerDataPackets = 0;
    }
    lastPlayerRacerData = racerData.clone();
    duplicateRaceDataDetected = consecutiveDuplicatePlayerDataPackets >= duplicateLimit;
  }

  public synchronized void resetRaceDataTracking() {
    latestReceivedRaceData = ReceivedRaceData.empty();
    lastReceivedCount = 0;
    lastPlayerRacerData = null;
    consecutiveDuplicatePlayerDataPackets = 0;
    duplicateRaceDataDetected = false;
    hasReceivedRaceData = false;
    cpuReallocationNeeded = false;
    cpuRestorationNeeded = false;
    cpuHandoffActive = false;
  }

  @Override
  public boolean equals(Object o) {
    if (!(o instanceof FalconnectClientConnection other)) return false;

    return other.socket.equals(socket);
  }
}
