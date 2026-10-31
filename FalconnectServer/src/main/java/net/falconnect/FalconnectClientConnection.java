package net.falconnect;

import net.falconnect.messages.MessageHandlerThread;
import net.falconnect.messages.MessageSenderThread;
import net.falconnect.messages.toclient.ToClientMessage;
import net.falconnect.messages.toclient.DisconnectMessage;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class FalconnectClientConnection {
  // The client restarts its race-data counter at one for every START_RACE. Permit a small loss
  // window for that first packet, but do not let a late high-numbered packet from the previous
  // race establish the new race's sequence high-water mark.
  private static final int MAX_INITIAL_RACE_PACKET_SEQUENCE = 128;
  public Socket socket;
  private volatile UdpTransport udpTransport;
  private volatile UUID udpSessionToken;
  private volatile InetSocketAddress udpEndpoint;
  public DataInputStream fromClientStream;
  private final DataOutputStream toClientStream;
  public volatile ClientState state;

  public int uid;
  public byte playerNum;
  public byte racerId;
  public byte selectedCourse;
  // Course voting is deliberately separate from final machine/settings readiness.
  public volatile boolean hasVotedTrack;
  public byte[] customMachineData;
  public byte[] name;
  public byte numCpus;
  public byte cpuStartIndex;
  private int lastReceivedCount;
  private boolean awaitingInitialRacePacket;
  public volatile long lastUdpPacketReceivedAt;
  private volatile long lastUdpHeartbeatAt;
  // A packet can briefly carry the prior CPU-slot count while a reassignment control message is
  // in flight. Track transport liveness separately from accepted racer data so that transition
  // cannot look like a disconnected client.
  public volatile long lastUdpRacePacketObservedAt;
  public volatile boolean cpuReallocationNeeded;
  public volatile boolean cpuRestorationNeeded;
  public volatile boolean hasReceivedRaceData;
  public volatile boolean cpuHandoffActive;
  public volatile boolean duplicateRaceDataDetected;
  private List<Byte> cpuRacerIndices = new ArrayList<>();
  private List<Byte> previousCpuRacerIndices = new ArrayList<>();
  private List<Byte> homeCpuRacerIndices = new ArrayList<>();
  private byte[] lastPlayerRacerData;
  private long duplicatePlayerRacerDataSince;
  private long lastRacePacketRejectionLogAt;

  public boolean hasUpdated = false;
  public volatile boolean disconnected = false;
  private volatile FalconnectRace race;

  private final MessageHandlerThread receiveMessageThread;
  private final MessageSenderThread sendMessageThread;

  private ReceivedRaceData latestReceivedRaceData = ReceivedRaceData.empty();
  public byte[] dataToSend;

  public FalconnectClientConnection(Socket socket) throws IOException {
    this(socket, true);
  }

  FalconnectClientConnection(Socket socket, boolean startThreads) throws IOException {
    this.socket = socket;
    if (startThreads) {
      try {
        ProtocolVersion.exchange(socket);
      } catch (IOException e) {
        socket.close();
        throw e;
      }
    }
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
    awaitingInitialRacePacket = true;

    receiveMessageThread = new MessageHandlerThread(this);
    sendMessageThread = new MessageSenderThread(this);
    if (startThreads) {
      receiveMessageThread.start();
      sendMessageThread.start();
      System.out.println("Message receiver thread started");
    }
  }

  public void SendPacket(byte[] packet) throws IOException, InterruptedException {
    if (disconnected) return;
    //Thread.sleep(4);

    if (packet[0] == ToClientPacketType.FULL_DATA.ordinal()) {
      SendUdpPacket(packet);
      return;
    }
    synchronized (toClientStream) {
      if (!disconnected) toClientStream.write(packet);
    }
  }

  public void SendUdpPacket(byte[] packet) throws IOException {
    UdpTransport transport = udpTransport;
    InetSocketAddress destination = udpEndpoint;
    if (!disconnected && transport != null && destination != null)
      transport.send(packet, destination);
  }

  public void SendMessage(ToClientMessage message) throws InterruptedException {
    if (!disconnected) sendMessageThread.messagesToSend.put(message);
  }

  public void Disconnect(String reason) {
    System.out.println("Sending DISCONNECT to player " + uid + ": " + reason);
    synchronized (toClientStream) {
      try {
        // Deliver the notice before stopping the queued sender. Serialize it with TCP frames.
        if (!disconnected) {
          new DisconnectMessage(this, reason).SendDataFromThread();
          System.out.println("Sent DISCONNECT frame to player " + uid);
        }
      } catch (IOException e) {
        System.out.println("Unable to send disconnect notice to player " + uid);
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
      } finally {
        disconnected = true;
      }
    }
    CloseUdpSession();
    try {
      socket.close();
    } catch (IOException e) {
      System.out.println("Unable to close TCP connection for player " + uid);
    }
  }

  public synchronized void SetUdpTransport(UdpTransport transport) {
    if (udpTransport == transport) return;
    if (udpTransport != null && udpSessionToken != null)
      udpTransport.unregister(udpSessionToken, this);
    udpTransport = transport;
    if (udpTransport != null && udpSessionToken != null)
      udpTransport.register(udpSessionToken, this);
  }

  public synchronized void RotateUdpSession() {
    if (udpTransport != null && udpSessionToken != null)
      udpTransport.unregister(udpSessionToken, this);
    udpEndpoint = null;
    udpSessionToken = disconnected ? null : UUID.randomUUID();
    if (udpTransport != null && udpSessionToken != null)
      udpTransport.register(udpSessionToken, this);
  }

  public synchronized void CloseUdpSession() {
    if (udpTransport != null && udpSessionToken != null)
      udpTransport.unregister(udpSessionToken, this);
    udpSessionToken = null;
    udpEndpoint = null;
  }

  public UUID getUdpSessionToken() { return udpSessionToken; }

  public synchronized boolean noteUdpEndpoint(InetSocketAddress endpoint, UUID token) {
    if (!token.equals(udpSessionToken) || disconnected) return false;
    udpEndpoint = endpoint;
    return true;
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

  // A race reset must not be interleaved with a UDP handler which has already compared its
  // sequence number but has not yet stored it. Otherwise, an in-flight packet from the previous
  // race can restore the old high-water mark after the reset and reject every packet in the new
  // race (whose sender starts again at sequence 1).
  public synchronized boolean acceptReceivedRaceData(List<byte[]> racerData,
                                                      List<Byte> cpuRacerIndices,
                                                      int sourceSequence,
                                                      long duplicateTimeoutMillis) {
    return acceptReceivedRaceData(racerData, cpuRacerIndices, sourceSequence,
        duplicateTimeoutMillis, null);
  }

  public synchronized boolean acceptReceivedRaceData(List<byte[]> racerData,
                                                      List<Byte> cpuRacerIndices,
                                                      int sourceSequence,
                                                      long duplicateTimeoutMillis,
                                                      UUID token) {
    if (token != null && !token.equals(udpSessionToken)) return false;
    if (awaitingInitialRacePacket) {
      if (sourceSequence > MAX_INITIAL_RACE_PACKET_SEQUENCE) return false;
      awaitingInitialRacePacket = false;
    }
    if (sourceSequence <= lastReceivedCount) return false;

    latestReceivedRaceData = new ReceivedRaceData(List.copyOf(racerData),
        List.copyOf(cpuRacerIndices), sourceSequence);
    lastReceivedCount = sourceSequence;
    // Unchanged human data is normal when only an owned CPU changed.
    hasUpdated = true;
    lastUdpPacketReceivedAt = System.currentTimeMillis();
    if (!hasReceivedRaceData) {
      System.out.println("Accepted first race packet from player " + playerNum +
          " sequence=" + sourceSequence);
    }
    hasReceivedRaceData = true;
    if (cpuHandoffActive) cpuRestorationNeeded = true;
    return true;
  }

  public synchronized void noteUdpHeartbeat() {
    lastUdpHeartbeatAt = System.currentTimeMillis();
    if (hasReceivedRaceData && cpuHandoffActive) cpuRestorationNeeded = true;
  }

  public synchronized boolean noteUdpHeartbeat(UUID token) {
    if (!token.equals(udpSessionToken)) return false;
    noteUdpHeartbeat();
    return true;
  }

  public long getLastUdpActivityAt() {
    return Math.max(lastUdpHeartbeatAt, lastUdpRacePacketObservedAt);
  }

  public long getLastAcceptedUdpActivityAt() {
    return Math.max(lastUdpHeartbeatAt, lastUdpPacketReceivedAt);
  }

  public void noteRacePacketObserved() {
    lastUdpRacePacketObservedAt = System.currentTimeMillis();
  }

  public synchronized boolean noteRacePacketObserved(UUID token) {
    if (!token.equals(udpSessionToken)) return false;
    noteRacePacketObserved();
    return true;
  }

  public synchronized void logRacePacketRejection(String reason) {
    long now = System.currentTimeMillis();
    if (now - lastRacePacketRejectionLogAt < 1_000) return;
    lastRacePacketRejectionLogAt = now;
    System.out.println("Ignoring race packet from player " + playerNum + ": " + reason);
  }

  public synchronized int getLastReceivedCount() {
    return lastReceivedCount;
  }

  public synchronized ReceivedRaceData getLatestReceivedRaceData() {
    return latestReceivedRaceData;
  }

  public synchronized List<Byte> getCpuRacerIndices() { return new ArrayList<>(cpuRacerIndices); }
  public FalconnectRace getRace() { return race; }
  public void setRace(FalconnectRace race) { this.race = race; }
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

  public synchronized void recordPlayerRacerData(byte[] racerData, long duplicateTimeoutMillis) {
    long now = System.currentTimeMillis();
    if (java.util.Arrays.equals(lastPlayerRacerData, racerData)) {
      if (duplicatePlayerRacerDataSince == 0) duplicatePlayerRacerDataSince = now;
    } else {
      duplicatePlayerRacerDataSince = 0;
    }
    lastPlayerRacerData = racerData.clone();
    duplicateRaceDataDetected = duplicatePlayerRacerDataSince != 0 &&
        now - duplicatePlayerRacerDataSince >= duplicateTimeoutMillis;
  }

  public synchronized void resetRaceDataTracking() {
    lastUdpHeartbeatAt = 0;
    latestReceivedRaceData = ReceivedRaceData.empty();
    lastReceivedCount = 0;
    awaitingInitialRacePacket = true;
    lastPlayerRacerData = null;
    duplicatePlayerRacerDataSince = 0;
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
