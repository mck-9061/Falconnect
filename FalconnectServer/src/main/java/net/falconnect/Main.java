package net.falconnect;

import java.io.IOException;

public class Main {
  public static FalconnectServer server;

  public static void main(String[] args) throws IOException, InterruptedException {
    System.out.println("Starting Falconnect server...");

    server = new FalconnectServer();

    while (true) {
      server.MainLoop();
      Thread.sleep(32);
    }
  }
}
