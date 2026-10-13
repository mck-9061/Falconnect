package net.falconnect;

/**
 * Wire layout shared with the C++ RacerMemoryBlock serializer. Each racer value is a 32-bit word.
 */
public final class RaceDataFormat {
  public static final int PACKET_HEADER_BYTES = 5;
  public static final int MAX_RACERS = 30;
  public static final int RACER_FIELD_COUNT =
      1 +  // magic
      1 +  // state
      3 +  // center position
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
      3;   // timing data (12 bytes from the secondary racer block)
  public static final int RACER_DATA_BYTES = RACER_FIELD_COUNT * Integer.BYTES;
  public static final int RACER_SEQUENCE_BYTES = Integer.BYTES;
  public static final int SERVER_RACER_RECORD_BYTES = RACER_DATA_BYTES + RACER_SEQUENCE_BYTES;
  public static final int FULL_RACE_PACKET_BYTES =
      PACKET_HEADER_BYTES + (MAX_RACERS * SERVER_RACER_RECORD_BYTES);

  // TCP control frames keep their existing fixed size. Race UDP packets are variable length.
  public static final int INPUT_DATA_BYTES = 14 * Integer.BYTES;
  public static final int MAX_RACE_DATAGRAM_BYTES = PACKET_HEADER_BYTES + MAX_RACERS * (6 + RACER_DATA_BYTES);
  private static final int[] INPUT_WORDS = {0, 22, 23, 24, 25, 26, 27, 28, 29, 18, 34, 36, 37, 38};

  private RaceDataFormat() {}

  public record RacerRecord(int racer, boolean full, int source, byte[] payload) {}

  public static boolean isNewSequence(int next, int previous) {
    return next - previous > 0;
  }

  public static java.util.List<RacerRecord> decode(byte[] packet, boolean fromServer) {
    if (packet.length < PACKET_HEADER_BYTES || packet.length > MAX_RACE_DATAGRAM_BYTES || packet[0] != 1)
      throw new IllegalArgumentException("Invalid race datagram");
    java.nio.ByteBuffer buffer = java.nio.ByteBuffer.wrap(packet);
    buffer.get();
    int sequence = buffer.getInt();
    var records = new java.util.ArrayList<RacerRecord>();
    boolean[] seen = new boolean[MAX_RACERS];
    while (buffer.hasRemaining()) {
      if (buffer.remaining() < (fromServer ? 6 : 2))
        throw new IllegalArgumentException("Truncated racer header");
      int racer = Byte.toUnsignedInt(buffer.get());
      int kind = Byte.toUnsignedInt(buffer.get());
      if (racer >= MAX_RACERS || seen[racer] || kind > 1)
        throw new IllegalArgumentException("Invalid racer record");
      seen[racer] = true;
      int source = fromServer ? buffer.getInt() : sequence;
      int length = kind == 1 ? RACER_DATA_BYTES : INPUT_DATA_BYTES;
      if (buffer.remaining() < length) throw new IllegalArgumentException("Truncated racer data");
      byte[] payload = new byte[length];
      buffer.get(payload);
      records.add(new RacerRecord(racer, kind == 1, source, payload));
    }
    return records;
  }

  public static byte[] inputsFromFull(byte[] full) {
    byte[] inputs = new byte[INPUT_DATA_BYTES];
    for (int i = 0; i < INPUT_WORDS.length; ++i)
      System.arraycopy(full, INPUT_WORDS[i] * 4, inputs, i * 4, 4);
    return inputs;
  }

  public static byte[] withInputs(byte[] full, byte[] inputs) {
    byte[] merged = full.clone();
    for (int i = 0; i < INPUT_WORDS.length; ++i)
      System.arraycopy(inputs, i * 4, merged, INPUT_WORDS[i] * 4, 4);
    return merged;
  }
}
