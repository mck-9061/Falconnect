package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.RaceDataFormat;
import net.falconnect.ToClientPacketType;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class NamesMessage extends ToClientMessage {
  private static final int NAME_BYTES = 32;

  public NamesMessage(FalconnectClientConnection destination, byte[][] names) {
    super(destination);

    data[0] = (byte) ToClientPacketType.NAMES.ordinal();

    int cursor = 1;
    for (int racerIndex = 0; racerIndex < RaceDataFormat.MAX_RACERS; racerIndex++) {
      byte[] name = Arrays.copyOf(names[racerIndex], NAME_BYTES);
      System.arraycopy(name, 0, data, cursor, NAME_BYTES);
      cursor += NAME_BYTES;
    }
  }
}
