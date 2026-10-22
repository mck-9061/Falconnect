package net.falconnect;

/**
 * Wire layout shared with the C++ RacerMemoryBlock serializer. Each racer value is a 32-bit word.
 */
public final class RaceDataFormat {
  // Client packets contain a type and sequence. Server packets additionally carry the
  // authoritative race clock, in milliseconds, immediately after that sequence.
  public static final int CLIENT_PACKET_HEADER_BYTES = 5;
  public static final int SERVER_PACKET_HEADER_BYTES = 9;
  public static final int SERVER_RACE_TIME_OFFSET = CLIENT_PACKET_HEADER_BYTES;
  public static final int MAX_RACERS = 30;
  public static final int RACER_FIELD_COUNT =
      1 +  // magic
      1 +  // state
      3 +  // center position
      3 +  // last center position
      3 +  // world velocity
      3 +  // world orientation
      3 +  // up vector
      3 +  // gravity orientation
      3 +  // speed, aerial tilt, energy
      3 +  // track orientation
      7 +  // inputs
      1 +  // side attack
      4 +  // speed/acceleration values
      1 +  // restore flag
      3 +  // timing data (12 bytes at secondary offset 0x738, used for final order)
      1 +  // current-lap frame count (written at secondary offset 0x6c0)
      1 +  // race-time frame count (first word of total time at secondary offset 0x744)
      1;   // sender-local interpolation frame count
  public static final int RACER_DATA_BYTES = RACER_FIELD_COUNT * Integer.BYTES;
  public static final int RACER_SEQUENCE_BYTES = Integer.BYTES;
  public static final int SERVER_RACER_RECORD_BYTES = RACER_DATA_BYTES + RACER_SEQUENCE_BYTES;
  public static final int CLIENT_FULL_RACE_PACKET_BYTES =
      CLIENT_PACKET_HEADER_BYTES + (MAX_RACERS * RACER_DATA_BYTES);
  public static final int FULL_RACE_PACKET_BYTES =
      SERVER_PACKET_HEADER_BYTES + (MAX_RACERS * SERVER_RACER_RECORD_BYTES);

  private RaceDataFormat() {}

  public static int packetBytesForRacers(int racerCount) {
    return CLIENT_PACKET_HEADER_BYTES + (racerCount * RACER_DATA_BYTES);
  }

  public static int serverRecordOffset(int racerIndex) {
    return SERVER_PACKET_HEADER_BYTES + (racerIndex * SERVER_RACER_RECORD_BYTES);
  }

  public static int raceTimeFrameCount(byte[] racerData) {
    // The final word is the sender-local interpolation timestamp.  The preceding word is the
    // game's raw race-time frame counter used only for the server's zero-to-positive start gate.
    int offset = (RACER_FIELD_COUNT - 2) * Integer.BYTES;
    return ((racerData[offset] & 0xff) << 24) |
        ((racerData[offset + 1] & 0xff) << 16) |
        ((racerData[offset + 2] & 0xff) << 8) |
        (racerData[offset + 3] & 0xff);
  }
}
