package net.falconnect;

import net.falconnect.messages.fromclient.FullDataMessage;
import net.falconnect.messages.fromclient.UdpPingMessage;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

/** One server UDP socket for every game and player. The UUID identifies the TCP session. */
public final class UdpTransport implements AutoCloseable {
  private final DatagramSocket socket;
  private final ConcurrentHashMap<UUID, FalconnectClientConnection> sessions =
      new ConcurrentHashMap<>();
  private final Thread receiver;

  public UdpTransport(int port) throws SocketException {
    socket = new DatagramSocket(port);
    receiver = new Thread(this::receiveLoop, "Falconnect UDP receiver");
    receiver.setDaemon(true);
    receiver.start();
  }

  void register(UUID token, FalconnectClientConnection client) {
    sessions.put(token, client);
  }

  void unregister(UUID token, FalconnectClientConnection client) {
    sessions.remove(token, client);
  }

  void send(byte[] data, InetSocketAddress destination) throws IOException {
    socket.send(new DatagramPacket(data, data.length, destination));
  }

  int localPort() { return socket.getLocalPort(); }

  private void receiveLoop() {
    byte[] buffer = new byte[RaceDataFormat.CLIENT_FULL_RACE_PACKET_BYTES + 1];
    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
    while (!socket.isClosed()) {
      try {
        packet.setLength(buffer.length);
        socket.receive(packet);
        dispatch(packet);
      } catch (SocketException e) {
        if (socket.isClosed()) return;
        System.out.println("UDP socket error: " + e.getMessage());
      } catch (IOException e) {
        System.out.println("UDP receive error: " + e.getMessage());
      } catch (RuntimeException e) {
        // A malformed packet must not stop reception for all players.
        System.out.println("Ignoring invalid UDP packet: " + e.getMessage());
      }
    }
  }

  void dispatch(DatagramPacket packet) {
    if (packet.getLength() < RaceDataFormat.CLIENT_UDP_HEADER_BYTES ||
        packet.getLength() > RaceDataFormat.CLIENT_FULL_RACE_PACKET_BYTES) return;
    byte[] bytes = packet.getData();
    int start = packet.getOffset();
    ByteBuffer tokenBytes = ByteBuffer.wrap(bytes, start + RaceDataFormat.CLIENT_PACKET_HEADER_BYTES,
        RaceDataFormat.UDP_SESSION_TOKEN_BYTES);
    UUID token = new UUID(tokenBytes.getLong(), tokenBytes.getLong());
    FalconnectClientConnection client = sessions.get(token);
    if (client == null || client.disconnected || !token.equals(client.getUdpSessionToken())) return;

    byte type = bytes[start];
    if (type != (byte) 1 && type != (byte) 6) return;
    // Keep the established parser layout: type, sequence, then racer records.
    byte[] data = new byte[packet.getLength() - RaceDataFormat.UDP_SESSION_TOKEN_BYTES];
    System.arraycopy(bytes, start, data, 0, RaceDataFormat.CLIENT_PACKET_HEADER_BYTES);
    System.arraycopy(bytes, start + RaceDataFormat.CLIENT_UDP_HEADER_BYTES, data,
        RaceDataFormat.CLIENT_PACKET_HEADER_BYTES,
        packet.getLength() - RaceDataFormat.CLIENT_UDP_HEADER_BYTES);
    if (!client.noteUdpEndpoint(new InetSocketAddress(packet.getAddress(), packet.getPort()), token))
      return;
    if (type == (byte) 1) new FullDataMessage(client, data, token).ProcessMessage();
    else new UdpPingMessage(client, data, token).ProcessMessage();
  }

  @Override public void close() {
    socket.close();
    sessions.clear();
  }
}
