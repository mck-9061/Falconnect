package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;

import java.util.Arrays;

public class NameMessage extends FromClientMessage {
  public NameMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    byte[] nameBytes = new byte[32];
    System.arraycopy(data, 1, nameBytes, 0, 32);

    System.out.println(Arrays.toString(nameBytes));

    origin.name = nameBytes;
  }
}
