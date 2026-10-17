package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.RaceDataFormat;
import net.falconnect.ToClientPacketType;

import java.nio.charset.StandardCharsets;
import java.util.List;

public class NamesMessage extends ToClientMessage {
  private static final int NAME_BYTES = 32;

  public NamesMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients) {
    super(destination);

    data[0] = (byte) ToClientPacketType.NAMES.ordinal();

    int cursor = 1;
    for (FalconnectClientConnection client : clients) {
      System.arraycopy(client.name, 0, data, cursor, NAME_BYTES);
      cursor += NAME_BYTES;
    }

    for (int racerIndex = clients.size(); racerIndex < RaceDataFormat.MAX_RACERS; racerIndex++) {
      byte[] cpuName = ("CPU" + (racerIndex - clients.size() + 1))
          .getBytes(StandardCharsets.US_ASCII);
      System.arraycopy(cpuName, 0, data, cursor, cpuName.length);
      cursor += NAME_BYTES;
    }
  }
}
