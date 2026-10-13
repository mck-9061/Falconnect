package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.RaceDataFormat;

public class FullDataMessage extends FromClientMessage {
  public FullDataMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    final java.util.List<RaceDataFormat.RacerRecord> records;
    try {
      records = RaceDataFormat.decode(data, false);
    } catch (IllegalArgumentException e) {
      return;
    }
    int sequence = java.nio.ByteBuffer.wrap(data, 1, 4).getInt();
    synchronized (origin) {
      if (origin.hasReceivedRaceData && !RaceDataFormat.isNewSequence(sequence, origin.lastReceivedCount))
        return;
      // Explicit global IDs prevent a delayed packet from relabeling CPUs after reassignment.
      // Ignore relinquished CPUs while accepting the human's liveness after a handoff.
      var owned = origin.getCpuRacerIndices();
      if (records.stream().noneMatch(record -> record.racer() == origin.playerNum - 1)) return;
      origin.publishRaceRecords(records.stream().filter(record ->
          record.racer() == origin.playerNum - 1 || owned.contains((byte) record.racer())).toList());
      origin.lastReceivedCount = sequence;
      origin.hasUpdated = true;
      origin.lastUdpPacketReceivedAt = System.currentTimeMillis();
      origin.hasReceivedRaceData = true;
      if (origin.cpuHandoffActive) origin.cpuRestorationNeeded = true;
      // Holding the same controls is normal. Only compare the once-per-second full snapshots
      // for the existing frozen-game watchdog (about 15 seconds of identical vehicle data).
      records.stream().filter(record -> record.racer() == origin.playerNum - 1 && record.full())
          .findFirst().ifPresent(record -> origin.recordPlayerRacerData(record.payload(), 15));
    }
  }
}
