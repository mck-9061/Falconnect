package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.RaceDataFormat;

import java.util.ArrayList;
import java.util.List;

public class FullDataMessage extends FromClientMessage {
  // A stopped emulator can keep its networking worker alive, repeatedly sending the last frame.
  // Use elapsed time rather than a packet count: the latter depends on the client's send rate.
  private static final long DUPLICATE_RACE_DATA_TIMEOUT_MILLIS = 5_000;
  public FullDataMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    List<byte[]> allData = new ArrayList<>();
    if (data.length < RaceDataFormat.CLIENT_PACKET_HEADER_BYTES ||
        (data.length - RaceDataFormat.CLIENT_PACKET_HEADER_BYTES) % RaceDataFormat.RACER_DATA_BYTES != 0) {
      origin.logRacePacketRejection("invalid length " + data.length);
      return;
    }
    origin.noteRacePacketObserved();
    int cpuCount = (data.length - RaceDataFormat.CLIENT_PACKET_HEADER_BYTES) / RaceDataFormat.RACER_DATA_BYTES - 1;
    List<Byte> cpuIndices = clientCpuIndicesForPacket(cpuCount);
    final boolean cpuLayoutIsStale = cpuIndices == null;
    if (cpuLayoutIsStale) {
      origin.logRacePacketRejection("CPU slots=" + cpuCount + ", expected current=" +
          origin.getCpuRacerIndices().size() + " or previous=" +
          origin.getPreviousCpuRacerIndices().size() + "; accepting human record only");
      // The first record is always the human racer. Keep that valid liveness/race data while
      // deliberately dropping ambiguous CPU records until the assignment control message wins.
      cpuIndices = List.of();
    }

    int count = (data[1] & 0xff) << 24;
    count |= (data[2] & 0xff) << 16;
    count |= (data[3] & 0xff) << 8;
    count |= (data[4] & 0xff);

    for (int i = 0; i < 1 + cpuCount; i++) {
      if (cpuLayoutIsStale && i != 0) break;
      byte[] racerData = new byte[RaceDataFormat.RACER_DATA_BYTES];
      System.arraycopy(
          data,
          RaceDataFormat.CLIENT_PACKET_HEADER_BYTES + (i * RaceDataFormat.RACER_DATA_BYTES),
          racerData,
          0,
          RaceDataFormat.RACER_DATA_BYTES);
      allData.add(racerData);
    }

    // Sequence validation, publication and duplicate tracking share the same lock as the
    // START_RACE reset. This prevents an in-flight packet from the prior race reintroducing its
    // sequence after that reset.
    if (!origin.acceptReceivedRaceData(allData, cpuIndices, count,
        DUPLICATE_RACE_DATA_TIMEOUT_MILLIS)) {
      origin.logRacePacketRejection("sequence=" + count + ", latest=" +
          origin.getLastReceivedCount());
    }
    else if (origin.getRace() != null) {
      origin.getRace().noteRaceFrameProgress(allData);
    }


    //System.out.println("Received data starting with " + data[1]);
  }

  private List<Byte> clientCpuIndicesForPacket(int cpuCount) {
    List<Byte> current = origin.getCpuRacerIndices();
    if (cpuCount == current.size()) return current;
    List<Byte> previous = origin.getPreviousCpuRacerIndices();
    return cpuCount == previous.size() ? previous : null;
  }
}
