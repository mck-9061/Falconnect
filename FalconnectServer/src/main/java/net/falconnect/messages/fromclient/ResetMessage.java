package net.falconnect.messages.fromclient;

import net.falconnect.ClientState;
import net.falconnect.FalconnectClientConnection;

public class ResetMessage extends FromClientMessage {
  public ResetMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    origin.state = ClientState.IN_MENUS;
  }
}
