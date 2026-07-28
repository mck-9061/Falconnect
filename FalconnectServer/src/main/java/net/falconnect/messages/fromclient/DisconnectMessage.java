package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;

public class DisconnectMessage extends FromClientMessage {
  public DisconnectMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    origin.disconnected = true;
  }
}
