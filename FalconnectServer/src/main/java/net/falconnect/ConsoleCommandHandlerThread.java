package net.falconnect;

import java.io.PrintStream;
import java.util.Locale;
import java.util.Scanner;

public class ConsoleCommandHandlerThread extends Thread {
  private final FalconnectServer server;
  private final Scanner input;
  private final PrintStream output;

  public ConsoleCommandHandlerThread(FalconnectServer server) {
    this(server, new Scanner(System.in), System.out);
  }

  ConsoleCommandHandlerThread(FalconnectServer server, Scanner input, PrintStream output) {
    this.server = server;
    this.input = input;
    this.output = output;
  }

  @Override
  public void run() {
    while (input.hasNextLine()) executeCommand(input.nextLine());
  }

  void executeCommand(String command) {
    if (command.isBlank()) return;
    String[] args = command.trim().split("\\s+");
    String opcode = args[0].toLowerCase(Locale.ROOT);
    try {
      switch (opcode) {
        case "help":
          requireArguments(args, 0, "help");
          printHelp();
          break;
        case "list":
          requireArguments(args, 0, "list");
          output.println("Limits: " + server.getMaxConcurrentRaces() + " games, "
              + server.getMaxClientsPerRace() + " players per game, "
              + server.getMaxCpusPerClient() + " CPUs per client for upcoming races.");
          output.println("-----Server game and player listing-----");
          for (FalconnectRace race : server.getRaces()) {
            Integer track = race.getTrackOverride();
            output.println("Game " + race.getPortBlockIndex() + ": " + race.getClients().size()
                + " players. State: " + race.gameState + ". Track override: "
                + (track == null ? "none" : track));
            for (FalconnectClientConnection client : race.getClients()) {
              output.println(client.uid + ". IP:" + client.socket.getRemoteSocketAddress()
                  + ". Name: " + new String(client.name) + ". State: " + client.state
                  + ". Disconnected: " + client.disconnected);
            }
            output.println();
          }
          output.println("-----End-----");
          break;
        case "kick":
          requireArguments(args, 1, "kick <player-id>");
          int uid = Integer.parseInt(args[1]);
          output.println(server.kickPlayer(uid) ? "Player has been kicked."
              : "No player with ID " + uid + ". Use list to see player IDs.");
          break;
        case "kickall":
          requireArguments(args, 0, "kickall");
          output.println("Kicked all players and closed " + server.closeAllGames() + " games.");
          break;
        case "maxgames":
          requireArguments(args, 1, "maxgames <count>");
          server.setMaxConcurrentRaces(Integer.parseInt(args[1]));
          output.println("Maximum concurrent games set to " + server.getMaxConcurrentRaces()
              + ". Existing games continue.");
          break;
        case "maxplayers":
          requireArguments(args, 1, "maxplayers <count>");
          server.setMaxClientsPerRace(Integer.parseInt(args[1]));
          output.println("Maximum players per game set to " + server.getMaxClientsPerRace()
              + ". Existing players remain connected.");
          break;
        case "maxcpus":
          requireArguments(args, 1, "maxcpus <count>");
          server.setMaxCpusPerClient(Integer.parseInt(args[1]));
          output.println("Maximum CPUs per client set to " + server.getMaxCpusPerClient()
              + ". Applies to races that have not started loading yet.");
          break;
        case "track":
          requireArguments(args, 2, "track <game-id> <track-id|clear>");
          int gameId = Integer.parseInt(args[1]);
          Integer trackId = args[2].equalsIgnoreCase("clear") ? null : parseTrackId(args[2]);
          server.setTrackOverride(gameId, trackId);
          output.println(trackId == null ? "Track override cleared for game " + gameId + "."
              : "Game " + gameId + " will use track ID " + trackId + ".");
          break;
        default:
          output.println("Unknown command. Type help for commands and arguments.");
      }
    } catch (NumberFormatException e) {
      output.println("Invalid integer argument. Type help for commands and arguments.");
    } catch (IllegalArgumentException e) {
      output.println(e.getMessage());
    }
  }

  private static int parseTrackId(String value) {
    return value.startsWith("0x") || value.startsWith("0X")
        ? Integer.parseInt(value.substring(2), 16) : Integer.parseInt(value);
  }

  private static void requireArguments(String[] args, int count, String usage) {
    if (args.length != count + 1) throw new IllegalArgumentException("Usage: " + usage);
  }

  private void printHelp() {
    output.println("list                              List game IDs, player IDs and current limits.");
    output.println("kick <player-id>                  Disconnect one player.");
    output.println("maxgames <count>                  Set concurrent game limit (1-"
        + FalconnectServer.MAX_SUPPORTED_RACES + "); existing games continue.");
    output.println("maxplayers <count>                Set player limit per game (2-30); existing players stay.");
    output.println("kickall                           Kick every player and close every game; server stays online.");
    output.println("track <game-id> <track-id|clear>   Override a waiting game's track, or restore voting.");
    output.println("                                  Track IDs: 0-255, decimal or 0x-prefixed hexadecimal.");
    output.println("maxcpus <count>                   Set CPUs per client (0-29) for races not yet loading.");
    output.println("help                              List commands and arguments.");
    output.println("Settings last until server restart. Game IDs come from list and may be reused after closure.");
  }
}
