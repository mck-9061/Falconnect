package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

public class ConnectedMessage extends ToClientMessage {
  public ConnectedMessage(FalconnectClientConnection destination) {
    super(destination);

    data[0] = (byte) ToClientPacketType.CONNECTED.ordinal();
    data[1] = destination.playerNum;
  }
}
