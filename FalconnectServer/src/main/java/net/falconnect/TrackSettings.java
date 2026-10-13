package net.falconnect;

import java.nio.charset.StandardCharsets;

/** Track-specific race settings. Lap counts start at 1; boost laps are zero-based. */
public record TrackSettings(int trackId, String name, int lapCount, int boostLap) {
  public static final int DEFAULT_LAP_COUNT = 3;
  public static final int DEFAULT_BOOST_LAP = 1;
  public static final int MAX_NAME_BYTES = 255;

  public TrackSettings {
    FalconnectServer.requireRange(trackId, 0, 255, "Track ID");
    FalconnectServer.requireRange(lapCount, 1, 255, "Lap count");
    FalconnectServer.requireRange(boostLap, 0, lapCount, "Boost lap");
    if (name == null || name.isBlank()) name = "Track " + trackId;
    if (name.getBytes(StandardCharsets.UTF_8).length > MAX_NAME_BYTES)
      throw new IllegalArgumentException("Track name must fit in 255 UTF-8 bytes.");
  }

  public static TrackSettings defaults(int trackId) {
    return new TrackSettings(trackId, "", DEFAULT_LAP_COUNT, DEFAULT_BOOST_LAP);
  }
}
