package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.util.List;

public class RacerIdsMessage extends ToClientMessage {
  private static final int CUSTOM_MACHINE_RACER_ID = 0x32;

  public RacerIdsMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients) {
    super(destination);

    data[0] = (byte) ToClientPacketType.RACER_IDS.ordinal();

    int i = 1;
    for (FalconnectClientConnection client : clients) {
      data[i] = client.usesCustomMachine()
          ? (byte) CUSTOM_MACHINE_RACER_ID
          : client.racerId;
      i++;
    }

    while (i < 31) {
      data[i] = (byte) CUSTOM_MACHINE_RACER_ID;
      i++;
    }
  }
}
