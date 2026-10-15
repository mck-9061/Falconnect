package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.FalconnectRace;
import net.falconnect.MachineSelection;
import net.falconnect.RaceDataFormat;
import net.falconnect.ToClientPacketType;

public class CustomMachineDataMessage extends ToClientMessage {
  public CustomMachineDataMessage(FalconnectClientConnection destination, FalconnectRace race) {
    super(destination);
    data[0] = (byte) ToClientPacketType.CUSTOM_MACHINE_DATA.ordinal();
    int cursor = 2;
    int ownIndex = Byte.toUnsignedInt(destination.playerNum) - 1;
    // Match RACER_IDS on the client: swap server slot zero with this player's slot.
    // Include CPUs, and skip standard machines in the compact part stream.
    for (int local = 0; local < RaceDataFormat.MAX_RACERS; local++) {
      int serverIndex = local == 0 ? ownIndex : local == ownIndex ? 0 : local;
      MachineSelection.Machine machine = race.getRaceMachine(serverIndex);
      if (machine.id() != 0x32) continue;
      data[cursor++] = machine.body();
      data[cursor++] = machine.cockpit();
      data[cursor++] = machine.booster();
    }
    data[1] = (byte) ((cursor - 2) / 3);
  }
}
