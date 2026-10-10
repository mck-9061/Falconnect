package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;
import net.falconnect.TrackSettings;

import java.nio.charset.StandardCharsets;

public class CourseMessage extends ToClientMessage {
  public CourseMessage(FalconnectClientConnection destination, TrackSettings track, byte cpuCount) {
    super(destination);

    data[0] = (byte) ToClientPacketType.COURSE.ordinal();
    data[1] = (byte) track.trackId();
    data[2] = cpuCount;
    // Keep the original COURSE prefix and fixed TCP frame size. New clients read this extension.
    data[3] = (byte) track.lapCount();
    data[4] = (byte) track.boostLap();
    byte[] name = track.name().getBytes(StandardCharsets.UTF_8);
    data[5] = (byte) name.length;
    System.arraycopy(name, 0, data, 6, name.length);
  }
}
