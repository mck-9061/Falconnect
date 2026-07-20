package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;

public abstract class FromClientMessage {
  FalconnectClientConnection origin;
  byte[] data;

  protected FromClientMessage(FalconnectClientConnection origin, byte[] data) {
    this.origin = origin;
    this.data = data;
  }

  public abstract void ProcessMessage();
}
