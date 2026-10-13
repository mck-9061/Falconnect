package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.RaceDataFormat;

import java.util.ArrayList;
import java.util.List;

public class FullDataMessage extends FromClientMessage {
  private static final int MAX_CONSECUTIVE_DUPLICATE_PLAYER_PACKETS = 5000;
  public FullDataMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    List<byte[]> allData = new ArrayList<>();
    if (data.length < RaceDataFormat.PACKET_HEADER_BYTES ||
        (data.length - RaceDataFormat.PACKET_HEADER_BYTES) % RaceDataFormat.RACER_DATA_BYTES != 0) return;
    int cpuCount = (data.length - RaceDataFormat.PACKET_HEADER_BYTES) / RaceDataFormat.RACER_DATA_BYTES - 1;
    List<Byte> cpuIndices = clientCpuIndicesForPacket(cpuCount);
    if (cpuIndices == null) return;

    int count = (data[1] & 0xff) << 24;
    count |= (data[2] & 0xff) << 16;
    count |= (data[3] & 0xff) << 8;
    count |= (data[4] & 0xff);

    for (int i = 0; i < 1 + cpuCount; i++) {
      byte[] racerData = new byte[RaceDataFormat.RACER_DATA_BYTES];
      System.arraycopy(
          data,
          RaceDataFormat.PACKET_HEADER_BYTES + (i * RaceDataFormat.RACER_DATA_BYTES),
          racerData,
          0,
          RaceDataFormat.RACER_DATA_BYTES);
      allData.add(racerData);
    }

    // Sequence validation, publication and duplicate tracking share the same lock as the
    // START_RACE reset. This prevents an in-flight packet from the prior race reintroducing its
    // sequence after that reset.
    if (!origin.acceptReceivedRaceData(allData, cpuIndices, count,
        MAX_CONSECUTIVE_DUPLICATE_PLAYER_PACKETS)) {
      System.out.println("Skipping old packet: " + count + ". Latest packet: "
          + origin.getLastReceivedCount());
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
