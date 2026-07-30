package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.util.List;

public class NamesMessage extends ToClientMessage {
  public NamesMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients) {
    super(destination);

    data[0] = (byte) ToClientPacketType.NAMES.ordinal();

    int cursor = 1;
    for (FalconnectClientConnection client : clients) {
      System.arraycopy(client.name, 0, data, cursor, 32);
      cursor += 32;
    }
  }
}
