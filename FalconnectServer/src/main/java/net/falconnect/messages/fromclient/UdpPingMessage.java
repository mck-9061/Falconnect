package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.io.IOException;

/** Echoes the client's ping sequence through the same UDP path used for race data. */
public class UdpPingMessage extends FromClientMessage {
  public UdpPingMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    if (data.length != 5) return;

    origin.noteUdpHeartbeat();
    data[0] = (byte) ToClientPacketType.UDP_PONG.ordinal();
    try {
      origin.SendUdpPacket(data);
    } catch (IOException ignored) {
      // The normal UDP timeout/disconnect path handles an unreachable client.
    }
  }
}
