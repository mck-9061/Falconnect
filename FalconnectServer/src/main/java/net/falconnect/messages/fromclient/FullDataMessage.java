package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;

public class FullDataMessage extends FromClientMessage {
  public FullDataMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    origin.setLastReceivedData(data);
    origin.hasUpdated = true;

    //System.out.println("Received data starting with " + data[1]);
  }
}
