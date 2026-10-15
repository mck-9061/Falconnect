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
    if (state < 0 || state >= ClientState.values().length) return;
    // A delayed readiness worker must not move a loading/gridded client backwards.
    if (state == ClientState.READY.ordinal() && origin.state != ClientState.IN_MENUS &&
        origin.state != ClientState.READY) return;
    origin.state = ClientState.values()[state];

    System.out.println(origin.state);
  }
}
