package net.falconnect.messages.fromclient;

import net.falconnect.ClientState;
import net.falconnect.FalconnectClientConnection;

public class UpdateStateMessage extends FromClientMessage {
  public UpdateStateMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    byte checkByte = data[2];
    if (checkByte != 1) return;

    byte state = data[1];
    origin.state = ClientState.values()[state];

    System.out.println(origin.state);
  }
}
