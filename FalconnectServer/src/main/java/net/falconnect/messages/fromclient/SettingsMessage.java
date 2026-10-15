package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ClientState;

public class SettingsMessage extends FromClientMessage {

  public SettingsMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    if (origin.state != ClientState.IN_MENUS && origin.state != ClientState.READY) return;
    origin.racerId = data[1];
    origin.selectedCourse = data[2];
    System.arraycopy(data, 3, origin.customMachineData, 0, origin.customMachineData.length);
    System.out.println("Received racer ID: " + origin.racerId);
  }
}
