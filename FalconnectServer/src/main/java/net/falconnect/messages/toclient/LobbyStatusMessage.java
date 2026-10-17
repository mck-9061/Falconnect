package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

public class LobbyStatusMessage extends ToClientMessage {
  public LobbyStatusMessage(FalconnectClientConnection destination, int phase, int completedCount,
                            int playerCount, int countdownSeconds, String trackName) {
    super(destination);
    data[0] = (byte) ToClientPacketType.LOBBY_STATUS.ordinal();
    data[1] = (byte) phase;
    data[2] = (byte) completedCount;
    data[3] = (byte) playerCount;
    data[4] = (byte) (countdownSeconds >>> 8);
    data[5] = (byte) countdownSeconds;
    byte[] name = trackName.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    int length = Math.min(name.length, data.length - 7);
    data[6] = (byte) length;
    System.arraycopy(name, 0, data, 7, length);
  }
}
