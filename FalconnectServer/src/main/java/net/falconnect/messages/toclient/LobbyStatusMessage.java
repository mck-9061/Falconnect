package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

public class LobbyStatusMessage extends ToClientMessage {
  public LobbyStatusMessage(FalconnectClientConnection destination, int readyCount, int playerCount) {
    super(destination);
    data[0] = (byte) ToClientPacketType.LOBBY_STATUS.ordinal();
    data[1] = (byte) readyCount;
    data[2] = (byte) playerCount;
  }
}
