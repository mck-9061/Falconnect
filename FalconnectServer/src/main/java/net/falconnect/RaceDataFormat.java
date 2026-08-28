package net.falconnect;

/**
 * Wire layout shared with the C++ RacerMemoryBlock serializer. Each racer value is a 32-bit word.
 */
public final class RaceDataFormat {
  public static final int PACKET_HEADER_BYTES = 5;
  public static final int MAX_RACERS = 30;
  public static final int RACER_FIELD_COUNT =
      1 +  // state
      3 +  // center position
      3 +  // world velocity
      3 +  // world orientation
      3 +  // up vector
      3 +  // gravity orientation
      3 +  // speed, aerial tilt, energy
      7 +  // inputs
      1 +  // side attack
      4 +  // speed/acceleration values
      1;   // restore flag
  public static final int RACER_DATA_BYTES = RACER_FIELD_COUNT * Integer.BYTES;
  public static final int FULL_RACE_PACKET_BYTES =
      PACKET_HEADER_BYTES + (MAX_RACERS * RACER_DATA_BYTES);

  private RaceDataFormat() {}

  public static int packetBytesForRacers(int racerCount) {
    return PACKET_HEADER_BYTES + (racerCount * RACER_DATA_BYTES);
  }
}
