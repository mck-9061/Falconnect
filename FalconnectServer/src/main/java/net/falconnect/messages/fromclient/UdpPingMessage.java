package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.io.IOException;
import java.util.UUID;

/** Echoes the client's ping sequence through the same UDP path used for race data. */
public class UdpPingMessage extends FromClientMessage {
  private final UUID udpSessionToken;
  public UdpPingMessage(FalconnectClientConnection origin, byte[] data) {
    this(origin, data, null);
  }

  public UdpPingMessage(FalconnectClientConnection origin, byte[] data, UUID token) {
    super(origin, data);
    udpSessionToken = token;
  }

  @Override
  public void ProcessMessage() {
    if (data.length != 5) return;

    if (udpSessionToken == null) origin.noteUdpHeartbeat();
    else if (!origin.noteUdpHeartbeat(udpSessionToken)) return;
    data[0] = (byte) ToClientPacketType.UDP_PONG.ordinal();
    try {
      origin.SendUdpPacket(data);
    } catch (IOException ignored) {
      // The normal UDP timeout/disconnect path handles an unreachable client.
    }
  }
}
