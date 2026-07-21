package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;

import java.util.List;

public class RacerIdsMessage extends ToClientMessage {
  public RacerIdsMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients) {
    super(destination);

    int i = 1;
    for (FalconnectClientConnection client : clients) {
      data[i] = client.racerId;
      i++;
    }
  }
}
