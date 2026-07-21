package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.util.List;

public class RacerIdsMessage extends ToClientMessage {
  public RacerIdsMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients) {
    super(destination);

    data[0] = (byte) ToClientPacketType.RACER_IDS.ordinal();

    int i = 1;
    for (FalconnectClientConnection client : clients) {
      data[i] = client.racerId;
      i++;
    }
  }
}
