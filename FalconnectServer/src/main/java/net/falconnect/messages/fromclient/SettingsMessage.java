package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;

public class SettingsMessage extends FromClientMessage {

  public SettingsMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    origin.racerId = data[1];
  }
}
