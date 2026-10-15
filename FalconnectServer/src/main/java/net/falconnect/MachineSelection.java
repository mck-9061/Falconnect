package net.falconnect;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Immutable console policy; selections are generated once per race for the entire grid. */
public record MachineSelection(Mode mode, int machineId, int body, int cockpit, int booster) {
  public enum Mode { SAME, CUSTOM, RANDOM, RANDOM_CUSTOM }
  public record Machine(byte id, byte body, byte cockpit, byte booster) {}

  public static MachineSelection same(int id) {
    FalconnectServer.requireRange(id, 0, 40, "Machine ID");
    return new MachineSelection(Mode.SAME, id, 0, 0, 0);
  }

  public static MachineSelection custom(int body, int cockpit, int booster) {
    FalconnectServer.requireRange(body, 0, 24, "Body ID");
    FalconnectServer.requireRange(cockpit, 25, 49, "Cockpit ID");
    FalconnectServer.requireRange(booster, 50, 74, "Booster ID");
    return new MachineSelection(Mode.CUSTOM, 0x32, body, cockpit, booster);
  }

  public static MachineSelection random(boolean custom) {
    return new MachineSelection(custom ? Mode.RANDOM_CUSTOM : Mode.RANDOM, 0, 0, 0, 0);
  }

  public List<Machine> generate() {
    List<Integer> pool = new ArrayList<>();
    int count = mode == Mode.RANDOM_CUSTOM ? 25 * 25 * 25 : 41;
    for (int i = 0; i < count; i++) pool.add(i);
    Collections.shuffle(pool);
    List<Machine> grid = new ArrayList<>();
    for (int i = 0; i < RaceDataFormat.MAX_RACERS; i++) {
      int value = pool.get(i);
      grid.add(switch (mode) {
        case SAME -> new Machine((byte) machineId, (byte) 0, (byte) 0, (byte) 0);
        case CUSTOM -> new Machine((byte) 0x32, (byte) body, (byte) cockpit, (byte) booster);
        case RANDOM -> new Machine((byte) value, (byte) 0, (byte) 0, (byte) 0);
        case RANDOM_CUSTOM -> new Machine((byte) 0x32, (byte) (value / 625),
            (byte) (25 + value / 25 % 25), (byte) (50 + value % 25));
      });
    }
    return List.copyOf(grid);
  }
}
