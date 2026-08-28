package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

public class ConnectedMessage extends ToClientMessage {
  public ConnectedMessage(FalconnectClientConnection destination) {
    super(destination);

    data[0] = (byte) ToClientPacketType.CONNECTED.ordinal();
    data[1] = destination.playerNum;
    data[2] = destination.numCpus;
    data[3] = destination.cpuStartIndex;
    data[4] = (byte) (destination.udpPort >>> 8);
    data[5] = (byte) destination.udpPort;
  }
}
