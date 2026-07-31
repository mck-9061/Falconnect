package net.falconnect;

import net.falconnect.messages.toclient.DisconnectMessage;

import java.util.Arrays;
import java.util.Scanner;

public class ConsoleCommandHandlerThread extends Thread {
  private FalconnectServer server;

  public ConsoleCommandHandlerThread(FalconnectServer server) {
    super();

    this.server = server;
  }

  public void run() {
    Scanner sc = new Scanner(System.in);

    while (true) {
      String command = sc.nextLine();

      String opcode = command.split(" ")[0];

      switch (opcode) {
        case "list": {
          System.out.println("-----Server race and player listing-----");
          int i = 0;
          for (FalconnectRace race : server.currentRaces) {
            System.out.println("Race " + i + ": " + race.getClients().size() + " players. State: " + race.gameState);
            for (FalconnectClientConnection client : race.getClients()) {
              System.out.println(client.uid + ". IP:" + client.socket.getRemoteSocketAddress() + ". Name: " + new String(client.name) + ". State: " + client.state + ". Disconnected: " + client.disconnected);
            }
            System.out.println();
          }
          System.out.println("-----End-----");

          break;
        }

        case "kick": {
          int uidToKick = Integer.parseInt(command.split(" ")[1].split("\n")[0]);
          FalconnectClientConnection client = null;

          for (FalconnectRace race : server.currentRaces) {
            for (FalconnectClientConnection c : race.getClients()) {
              if (c.uid == uidToKick) {
                client = c;
                break;
              }
            }

            if (client != null) break;
          }

          DisconnectMessage message = new DisconnectMessage(client, "You have been kicked from the server.");
          try {
            message.Send();
          } catch (InterruptedException e) {
            throw new RuntimeException(e);
          }
          client.disconnected = true;

          System.out.println("Player has been kicked");

          break;
        }

        default: {
          System.out.println("Unknown command!");
          break;
        }
      }
    }
  }
}
