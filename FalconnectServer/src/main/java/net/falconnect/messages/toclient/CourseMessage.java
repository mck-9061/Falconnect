package net.falconnect.messages.toclient;

import net.falconnect.FalconnectClientConnection;
import net.falconnect.ToClientPacketType;

public class CourseMessage extends ToClientMessage {
  public CourseMessage(FalconnectClientConnection destination, byte courseID, byte cpuCount) {
    super(destination);

    data[0] = (byte) ToClientPacketType.COURSE.ordinal();
    data[1] = courseID;
    data[2] = cpuCount;
    //data[2] = 29;
  }
}
