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
          output.println("Track vote countdown: " + server.getTrackVoteCountdownSeconds()
              + " seconds. Machine countdown: " + server.getCountdownSeconds() + " seconds.");
          output.println("-----Server game and player listing-----");
          for (FalconnectRace race : server.getRaces()) {
            Integer track = race.getTrackOverride();
            output.println("Game " + race.getPortBlockIndex() + ": " + race.getClients().size()
                + " players. State: " + race.gameState + ". Track override: "
                + (track == null ? "none" : track)
                + ". Lap count override: " + formatOverride(race.getLapCountOverride())
                + ". Boost lap override: " + formatOverride(race.getBoostLapOverride())
                + ". Machine override: " + race.getMachineOverride());
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
        case "countdown":
          requireArguments(args, 1, "countdown <seconds>");
          server.setCountdownSeconds(Integer.parseInt(args[1]));
          output.println("Machine countdown set to " + server.getCountdownSeconds()
              + " seconds from the first ready player, including waiting games.");
          break;
        case "trackcountdown":
          requireArguments(args, 1, "trackcountdown <seconds>");
          server.setTrackVoteCountdownSeconds(Integer.parseInt(args[1]));
          output.println("Track vote countdown set to " + server.getTrackVoteCountdownSeconds()
              + " seconds from the first vote, including waiting games.");
          break;
        case "skiptrack":
        case "skipvote":
          requireArguments(args, 1, opcode + " <game-id>");
          server.skipTrackVoteCountdown(Integer.parseInt(args[1]));
          output.println("Skipped the track vote countdown for game " + args[1] + ".");
          break;
        case "skipready":
        case "skipcountdown":
          requireArguments(args, 1, opcode + " <game-id>");
          server.skipReadyCountdown(Integer.parseInt(args[1]));
          output.println("Skipped the machine countdown for game " + args[1] + ".");
          break;
        case "maxcpus":
          requireArguments(args, 1, "maxcpus <count>");
          server.setMaxCpusPerClient(Integer.parseInt(args[1]));
          output.println("Maximum CPUs per client set to " + server.getMaxCpusPerClient()
              + ". Applies to races that have not started loading yet.");
          break;
        case "machine": {
          requireArguments(args, 2, "machine <game-id> <machine-id|random|clear>");
          int gameId = Integer.parseInt(args[1]);
          MachineSelection selection = args[2].equalsIgnoreCase("clear") ? null
              : args[2].equalsIgnoreCase("random") ? MachineSelection.random(false)
              : MachineSelection.same(parseTrackId(args[2]));
          server.setMachineOverride(gameId, selection);
          output.println("Machine override for game " + gameId + ": " + selection);
          break;
        }
        case "custommachine": {
          if (args.length != 3 && args.length != 5)
            throw new IllegalArgumentException("Usage: custommachine <game-id> <body-id> <cockpit-id> <booster-id> | custommachine <game-id> <random|clear>");
          int gameId = Integer.parseInt(args[1]);
          MachineSelection selection;
          if (args.length == 5)
            selection = MachineSelection.custom(parseTrackId(args[2]), parseTrackId(args[3]), parseTrackId(args[4]));
          else if (args[2].equalsIgnoreCase("random")) selection = MachineSelection.random(true);
          else if (args[2].equalsIgnoreCase("clear")) selection = null;
          else throw new IllegalArgumentException("Use random, clear, or three part IDs.");
          server.setMachineOverride(gameId, selection);
          output.println("Machine override for game " + gameId + ": " + selection);
          break;
        }
        case "track": {
          requireArguments(args, 2, "track <game-id> <track-id|random|clear>");
          int gameId = Integer.parseInt(args[1]);
          if (args[2].equalsIgnoreCase("random")) {
            output.println("Game " + gameId + " will use randomly selected track ID "
                + server.selectRandomTrack(gameId) + ".");
            break;
          }
          Integer trackId = args[2].equalsIgnoreCase("clear") ? null : parseTrackId(args[2]);
          server.setTrackOverride(gameId, trackId);
          output.println(trackId == null ? "Track override cleared for game " + gameId + "."
              : "Game " + gameId + " will use track ID " + trackId + ".");
          break;
        }
        case "laps":
        case "boostlap": {
          requireArguments(args, 2, opcode + " <game-id> <" + (opcode.equals("laps") ? "count" : "lap") + "|clear>");
          int gameId = Integer.parseInt(args[1]);
          Integer value = args[2].equalsIgnoreCase("clear") ? null : Integer.parseInt(args[2]);
          if (opcode.equals("laps")) server.setLapCountOverride(gameId, value);
          else server.setBoostLapOverride(gameId, value);
          String setting = opcode.equals("laps") ? "Lap count" : "Boost lap";
          output.println(value == null ? setting + " override cleared for game " + gameId + "."
              : setting + " override for game " + gameId + " set to " + value + ".");
          break;
        }
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

  private static String formatOverride(Integer value) {
    return value == null ? "track default" : value.toString();
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
    output.println("track <game-id> <track-id|random|clear>   Override a waiting game's track and skip voting, or restore voting.");
    output.println("                                  Track IDs: 0-255, decimal or 0x-prefixed hexadecimal.");
    output.println("                                  random selects a compatible track from TrackTable now.");
    output.println("machine <game-id> <id|random|clear> Force all humans and CPUs to one machine, unique random machines, or restore selections.");
    output.println("                                  Machine IDs: 0-40 (0x00-0x28); random draws without replacement.");
    output.println("custommachine <game-id> <body> <cockpit> <booster> Force custom parts for every human and CPU.");
    output.println("                                  Body: 0-24; cockpit: 25-49; booster: 50-74. Decimal or 0x hex.");
    output.println("custommachine <game-id> <random|clear> Unique random part combinations, or restore selections.");
    output.println("                                  Machine overrides apply to waiting games and reroll each race until cleared.");
    output.println("laps <game-id> <count|clear>       Override a waiting game's lap count (1-255), or use the track default.");
    output.println("boostlap <game-id> <lap|clear>     Override its boost lap (0-255, zero-based), or use the track default.");
    output.println("                                  0 grants boost from the start; boost lap cannot exceed the lap count.");
    output.println("                                  Incompatible track votes keep the game waiting until settings are corrected.");
    output.println("maxcpus <count>                   Set CPUs per client (0-29) for races not yet loading.");
    output.println("trackcountdown <seconds>         Choose a voted track after the first vote (1-3600 seconds; default 20).");
    output.println("skiptrack <game-id>              Skip that game's track vote countdown now.");
    output.println("countdown <seconds>              Start after the first machine selection (1-3600 seconds; default 20).");
    output.println("skipready <game-id>              Skip that game's machine-selection countdown now.");
    output.println("help                              List commands and arguments.");
    output.println("Global limits and countdowns persist in falconnect-server.properties. "
        + "Game IDs come from list and may be reused after closure.");
  }
}
