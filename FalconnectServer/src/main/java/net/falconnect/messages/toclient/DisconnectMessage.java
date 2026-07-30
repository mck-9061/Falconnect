package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

public class DisconnectMessage extends ToClientMessage {

  public DisconnectMessage(FalconnectClientConnection destination, String message) {
    super(destination);

    data[0] = (byte) ToClientPacketType.DISCONNECT.ordinal();

    int i = 1;
    for (char c : message.toCharArray()) {
      data[i] = (byte) c;
      i++;
    }
  }
}
