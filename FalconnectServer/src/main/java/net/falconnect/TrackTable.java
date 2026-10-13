package net.falconnect;

import java.util.HashMap;
import java.util.Map;

/** Edit the rows below, then rebuild and restart the server. */
public final class TrackTable {
  // A blank name is displayed as "Track <ID>". IDs may be written in decimal or hex.
  // Laps is 1 based and boost lap is 0-based; boost lap must be between 0 and the lap count.
  // Example row: new TrackSettings(0x12, "Your track name", 5, 2),
  // Defaults preserve the existing 3-lap race and SetBoostLap(1) behaviour.
  private static final TrackSettings[] ROWS = {
      //                ID    Name                                Laps  Boost lap
      new TrackSettings(0x01, "Mute City: Twist Road",            5,    1),
      new TrackSettings(0x03, "Mute City: Serial Gaps",           5,    1),
      new TrackSettings(0x05, "Aeropolis: Multiplex",             4,    1),
      new TrackSettings(0x07, "Port Town: Aero Dive",             4,    1),
      new TrackSettings(0x08, "Lightning: Loop Cross",            4,    1),
      new TrackSettings(0x09, "Lightning: Half Pipe",             3,    1),
      new TrackSettings(0x0A, "Green Plant: Intersection",        4,    1),
      new TrackSettings(0x0B, "Green Plant: Mobius Ring",         5,    1),
      new TrackSettings(0x0D, "Port Town: Long Pipe",             3,    1),
      new TrackSettings(0x0E, "Big Blue: Drift Highway",          5,    1),
      new TrackSettings(0x0F, "Fire Field: Cylinder Knot",        3,    1),
      new TrackSettings(0x10, "Casino Palace: Split Oval",        6,    1),
      new TrackSettings(0x11, "Fire Field: Undulation",           4,    1),
      new TrackSettings(0x15, "Aeropolis: Dragon Slope",          3,    1),
      new TrackSettings(0x18, "Cosmo Terminal: Trident",          3,    1),
      new TrackSettings(0x19, "Sand Ocean: Lateral Shift",        5,    1),
      new TrackSettings(0x1A, "Sand Ocean: Surface Slide",        4,    1),
      new TrackSettings(0x1B, "Big Blue: Ordeal",                 4,    1),
      new TrackSettings(0x1C, "Phantom Road: Slim-Line Slits",    3,    1),
      new TrackSettings(0x1D, "Casino Palace: Double Branches",   3,    1),
      new TrackSettings(0x1F, "Aeropolis: Screw Drive",           5,    1),
      new TrackSettings(0x20, "Outer Space: Meteor Stream",       5,    1),
      new TrackSettings(0x21, "Port Town: Cylinder Wave",         3,    1),
      new TrackSettings(0x22, "Lightning: Thunder Road",          3,    1),
      new TrackSettings(0x23, "Green Plant: Spiral",              3,    1),
      new TrackSettings(0x24, "Mute City: Sonic Oval",            7,    1),
      new TrackSettings(0x25, "Mute City: Training Grounds",      7,    1),
      new TrackSettings(0x26, "Red Canyon: Boulder Run",          1,    0),
      new TrackSettings(0x27, "Casino Palace: Shady Bar",         5,    1),
      new TrackSettings(0x28, "Mute City: Outskirts",             1,    0),
      new TrackSettings(0x29, "Lightning: Power Plant Exit",      1,    0),
      new TrackSettings(0x2A, "Port Town: Speed Trap",            1,    0),
      new TrackSettings(0x2B, "Mute City: Grand Prix",            5,    1),
      new TrackSettings(0x2C, "Underworld: Underground Secrets",  4,    1),
      new TrackSettings(0x2D, "Phantom Road: Creators' Domain",   4,    1),
  };

  private static final Map<Integer, TrackSettings> BY_ID = buildTable();

  private static Map<Integer, TrackSettings> buildTable() {
    Map<Integer, TrackSettings> table = new HashMap<>();
    for (TrackSettings track : ROWS) {
      if (table.putIfAbsent(track.trackId(), track) != null)
        throw new IllegalArgumentException("Duplicate track ID: " + track.trackId());
    }
    return Map.copyOf(table);
  }

  public static TrackSettings get(int trackId) {
    FalconnectServer.requireRange(trackId, 0, 255, "Track ID");
    return BY_ID.getOrDefault(trackId, TrackSettings.defaults(trackId));
  }

  private TrackTable() {}
}
