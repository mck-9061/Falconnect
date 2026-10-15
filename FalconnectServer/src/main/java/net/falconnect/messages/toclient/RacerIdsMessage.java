package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.FalconnectRace;
import net.falconnect.ToClientPacketType;

import java.util.List;

public class RacerIdsMessage extends ToClientMessage {

  public RacerIdsMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients,
                         FalconnectRace race) {
    super(destination);

    data[0] = (byte) ToClientPacketType.RACER_IDS.ordinal();

    for (int i = 0; i < 30; i++) data[i + 1] = race.getRaceMachine(i).id();
  }
}
