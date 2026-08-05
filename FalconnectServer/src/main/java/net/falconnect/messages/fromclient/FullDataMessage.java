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

    for (int i = 0; i < 1 + origin.numCpus; i++) {
      byte[] racerData = new byte[255];
      System.arraycopy(data, 1 + (i * 255), racerData, 0, 255);
      allData.add(racerData);
    }

    origin.setLastReceivedData(allData);
    origin.hasUpdated = true;

    //System.out.println("Received data starting with " + data[1]);
  }
}
