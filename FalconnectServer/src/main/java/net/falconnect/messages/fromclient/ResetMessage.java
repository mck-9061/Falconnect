package net.falconnect.messages.fromclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.FalconnectRace;

import java.io.IOException;

public class ResetMessage extends FromClientMessage {
  public ResetMessage(FalconnectClientConnection origin, byte[] data) {
    super(origin, data);
  }

  @Override
  public void ProcessMessage() {
    FalconnectRace race = origin.getRace();
    if (race == null) return;

    try {
      race.ReturnClientToLobby(origin);
    } catch (IOException e) {
      System.out.println("Unable to return player " + origin.playerNum + " to the lobby: "
          + e.getMessage());
      origin.Disconnect("Unable to join a new lobby.");
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
    }
  }
}
