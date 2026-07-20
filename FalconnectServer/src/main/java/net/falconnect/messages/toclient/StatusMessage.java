package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

public class StatusMessage extends ToClientMessage {
  public StatusMessage(FalconnectClientConnection destination, ToClientPacketType status) {
    super(destination);

    data[0] = (byte) status.ordinal();
  }
}
