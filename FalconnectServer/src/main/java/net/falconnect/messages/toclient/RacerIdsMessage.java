package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

import java.util.List;
import java.util.Random;

public class RacerIdsMessage extends ToClientMessage {
  public static boolean shouldRandomise = false;
  private static int[] cpuIds;

  public RacerIdsMessage(FalconnectClientConnection destination, List<FalconnectClientConnection> clients) {
    super(destination);

    if (shouldRandomise) {
      shouldRandomise = false;
      Random random = new Random();
      cpuIds = new int[30];
      for (int i = 0; i < 30; i++) {
        cpuIds[i] = random.nextInt(0, 41);
      }
    }

    data[0] = (byte) ToClientPacketType.RACER_IDS.ordinal();

    int i = 1;
    for (FalconnectClientConnection client : clients) {
      data[i] = client.racerId;
      i++;
    }

    while (i < 31) {
      data[i] = (byte) cpuIds[i - 1];
      i++;
    }
  }
}
