package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.RaceDataFormat;
import net.falconnect.ToClientPacketType;

import java.nio.charset.StandardCharsets;
import java.util.List;

public class NamesMessage extends ToClientMessage {
  private static final int NAME_BYTES = 32;
  private static final byte[] CPU_PREFIX = "CPU (".getBytes(StandardCharsets.US_ASCII);

  public NamesMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients) {
    super(destination);

    data[0] = (byte) ToClientPacketType.NAMES.ordinal();

    int cursor = 1;
    for (FalconnectClientConnection client : clients) {
      System.arraycopy(client.name, 0, data, cursor, NAME_BYTES);
      cursor += NAME_BYTES;
    }

    for (int racerIndex = clients.size(); racerIndex < RaceDataFormat.MAX_RACERS; racerIndex++) {
      FalconnectClientConnection owner = cpuOwner(clients, racerIndex);
      if (owner == null) {
        System.arraycopy("CPU".getBytes(StandardCharsets.US_ASCII), 0, data, cursor, 3);
      } else {
        writeCpuOwnerName(cursor, owner.name);
      }
      cursor += NAME_BYTES;
    }
  }

  private static FalconnectClientConnection cpuOwner(List<FalconnectClientConnection> clients,
                                                     int racerIndex) {
    byte index = (byte) racerIndex;
    for (FalconnectClientConnection client : clients) {
      if (client.getCpuRacerIndices().contains(index)) return client;
    }
    return null;
  }

  private void writeCpuOwnerName(int offset, byte[] playerName) {
    System.arraycopy(CPU_PREFIX, 0, data, offset, CPU_PREFIX.length);
    int nameLength = 0;
    int maxNameLength = NAME_BYTES - CPU_PREFIX.length - 1; // Reserve the closing parenthesis.
    while (nameLength < playerName.length && nameLength < maxNameLength && playerName[nameLength] != 0) {
      data[offset + CPU_PREFIX.length + nameLength] = playerName[nameLength];
      nameLength++;
    }
    data[offset + CPU_PREFIX.length + nameLength] = ')';
  }
}
