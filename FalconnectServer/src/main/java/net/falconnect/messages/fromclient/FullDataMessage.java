package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;

import java.util.ArrayList;
import java.util.List;

public class FullDataMessage extends FromClientMessage {
  public FullDataMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    List<byte[]> allData = new ArrayList<>();

    int count = (data[1] & 0xff) << 24;
    count |= (data[2] & 0xff) << 16;
    count |= (data[3] & 0xff) << 8;
    count |= (data[4] & 0xff);

    if (count > origin.lastReceivedCount) {
      origin.lastReceivedCount = count;
      for (int i = 0; i < 1 + origin.numCpus; i++) {
        byte[] racerData = new byte[124];
        System.arraycopy(data, 5 + (i * 124), racerData, 0, 124);
        allData.add(racerData);
      }

      origin.setLastReceivedData(allData);
      origin.hasUpdated = true;
    } else {
      System.out.println("Skipping old packet");
    }



    //System.out.println("Received data starting with " + data[1]);
  }
}
