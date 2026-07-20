package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;

public class FullDataMessage extends ToClientMessage {
  public FullDataMessage(FalconnectClientConnection destination, byte[] data) {
    super(destination);

    this.data = data;
  }
}
