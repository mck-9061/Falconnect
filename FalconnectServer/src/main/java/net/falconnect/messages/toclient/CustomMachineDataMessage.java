package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.util.List;

public class CustomMachineDataMessage extends ToClientMessage {
  private static final int COMPONENTS_PER_MACHINE = 3;

  public CustomMachineDataMessage(FalconnectClientConnection destination,
                                  List<FalconnectClientConnection> clients) {
    super(destination);
    data[0] = (byte) ToClientPacketType.CUSTOM_MACHINE_DATA.ordinal();

    int cursor = 2;

    // This is the complete custom-machine stream for this client, rather than only the
    // remote portion.  The game consumes it in its local racer order: its own racer first,
    // then the other player racers in the server's (UDP) order, followed by CPUs.  Keeping
    // that ordering here gives the client one authoritative stream to place at 0x801b0000.
    // In particular, it must not prepend its locally-read selection to this data.
    if (destination.usesCustomMachine()) {
      System.arraycopy(destination.customMachineData, 0, data, cursor, COMPONENTS_PER_MACHINE);
      cursor += COMPONENTS_PER_MACHINE;
    }

    for (FalconnectClientConnection client : clients) {
      if (client == destination || !client.usesCustomMachine())
        continue;

      System.arraycopy(client.customMachineData, 0, data, cursor, COMPONENTS_PER_MACHINE);
      cursor += COMPONENTS_PER_MACHINE;
    }

    data[1] = (byte) ((cursor - 2) / COMPONENTS_PER_MACHINE);
  }
}
