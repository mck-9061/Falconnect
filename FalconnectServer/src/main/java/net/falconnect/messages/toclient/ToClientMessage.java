package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.RaceDataFormat;

import java.io.IOException;

public abstract class ToClientMessage {
  FalconnectClientConnection destination;
  byte[] data;

  protected ToClientMessage(FalconnectClientConnection destination) {
    this.destination = destination;
    data = new byte[RaceDataFormat.FULL_RACE_PACKET_BYTES];
  }

  public void SendDataFromThread() throws IOException, InterruptedException {
    destination.SendPacket(data);
  }

  public void Send() throws InterruptedException {
    destination.SendMessage(this);
  }
}
