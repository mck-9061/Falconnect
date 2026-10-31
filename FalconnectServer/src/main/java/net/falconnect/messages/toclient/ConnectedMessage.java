package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;
import net.falconnect.FalconnectServer;
import java.nio.ByteBuffer;
import java.util.UUID;

public class ConnectedMessage extends ToClientMessage {
  public ConnectedMessage(FalconnectClientConnection destination) {
    super(destination);

    data[0] = (byte) ToClientPacketType.CONNECTED.ordinal();
    data[1] = destination.playerNum;
    data[2] = destination.numCpus;
    data[3] = destination.cpuStartIndex;
    data[4] = (byte) (FalconnectServer.UDP_PORT >>> 8);
    data[5] = (byte) FalconnectServer.UDP_PORT;
    UUID token = destination.getUdpSessionToken();
    if (token == null) throw new IllegalStateException("UDP session is not initialized");
    ByteBuffer.wrap(data, 6, 16).putLong(token.getMostSignificantBits())
        .putLong(token.getLeastSignificantBits());
  }
}
