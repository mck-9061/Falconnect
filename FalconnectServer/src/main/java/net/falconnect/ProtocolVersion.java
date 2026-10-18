package net.falconnect;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.concurrent.TimeUnit;

public final class ProtocolVersion {
  // Bump together with Core/Falconnect/ProtocolVersion.h for incompatible TCP or UDP changes.
  public static final int VERSION = 10;
  public static final int VERSION_MESSAGE = 0x7f;
  public static final int TIMEOUT_MILLIS = 5000;

  private ProtocolVersion() {}

  public static void exchange(Socket socket) throws IOException {
    exchange(socket, TIMEOUT_MILLIS);
  }

  static void exchange(Socket socket, int timeoutMillis) throws IOException {
    int originalTimeout = socket.getSoTimeout();
    long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMillis);
    try {
      DataOutputStream output = new DataOutputStream(socket.getOutputStream());
      output.writeByte(VERSION_MESSAGE);
      output.writeInt(VERSION);
      output.flush();

      DataInputStream input = new DataInputStream(socket.getInputStream());
      if (readByte(socket, input, deadline) != VERSION_MESSAGE)
        throw new IOException("Client did not send a protocol version message; update the client");
      int clientVersion = 0;
      for (int i = 0; i < 4; i++)
        clientVersion = (clientVersion << 8) | readByte(socket, input, deadline);
      if (clientVersion != VERSION)
        throw new IOException("Protocol version mismatch: client "
            + Integer.toUnsignedString(clientVersion) + ", server " + VERSION);
    } finally {
      socket.setSoTimeout(originalTimeout);
    }
  }

  private static int readByte(Socket socket, DataInputStream input, long deadline) throws IOException {
    long remaining = deadline - System.nanoTime();
    if (remaining <= 0) throw new SocketTimeoutException("Protocol version exchange timed out");
    socket.setSoTimeout((int) Math.max(1, TimeUnit.NANOSECONDS.toMillis(remaining)));
    return input.readUnsignedByte();
  }
}
