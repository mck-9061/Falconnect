package net.falconnect;

public class HackyAssSoftlockPreventionThread extends Thread {
  private FalconnectRace race;

  public HackyAssSoftlockPreventionThread(FalconnectRace race) {
    this.race = race;
  }

  public void run() {
    while (race.isRunning) {
      try {
        Thread.sleep(500);
      } catch (InterruptedException e) {
        throw new RuntimeException(e);
      }
      for (FalconnectClientConnection clientConnection : race.getClients()) {
        clientConnection.hasUpdated = true;
      }
    }
  }
}
