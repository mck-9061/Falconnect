package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.util.List;

public class CpuAssignmentMessage extends ToClientMessage {
  public CpuAssignmentMessage(FalconnectClientConnection destination) {
    super(destination);
    List<Byte> cpuIndices = destination.getCpuRacerIndices();
    data[0] = (byte) ToClientPacketType.CPU_ASSIGNMENT.ordinal();
    data[1] = (byte) cpuIndices.size();
    for (int i = 0; i < cpuIndices.size(); i++) data[i + 2] = cpuIndices.get(i);
  }
}
