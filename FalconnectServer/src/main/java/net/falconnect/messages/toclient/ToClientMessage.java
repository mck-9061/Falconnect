package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;

import java.io.IOException;

public abstract class ToClientMessage {
  FalconnectClientConnection destination;
  byte[] data;

  protected ToClientMessage(FalconnectClientConnection destination) {
    this.destination = destination;
    data = new byte[7680];
  }

  public void SendDataFromThread() throws IOException {
    destination.SendPacket(data);
  }

  public void Send() throws InterruptedException {
    destination.SendMessage(this);
  }
}
