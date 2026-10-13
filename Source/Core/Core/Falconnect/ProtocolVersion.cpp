#include "Core/Falconnect/ProtocolVersion.h"

#include <array>
#include <cerrno>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#endif

#include <fmt/format.h>

namespace Falconnect
{
std::string ExchangeProtocolVersion(int socket, std::chrono::milliseconds timeout)
{
#ifndef _WIN32
  if (socket < 0 || socket >= FD_SETSIZE)
    return "Unable to check the server protocol version: socket limit exceeded.";
#endif
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  const auto wait_for_socket = [&](bool writing) {
    while (true)
    {
      const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
          deadline - std::chrono::steady_clock::now());
      if (remaining.count() <= 0)
        return false;
      timeval wait{};
      wait.tv_sec = static_cast<decltype(wait.tv_sec)>(remaining.count() / 1000000);
      wait.tv_usec = static_cast<decltype(wait.tv_usec)>(remaining.count() % 1000000);
      fd_set sockets;
      FD_ZERO(&sockets);
      FD_SET(socket, &sockets);
      const int result = select(socket + 1, writing ? nullptr : &sockets,
                                writing ? &sockets : nullptr, nullptr, &wait);
#ifndef _WIN32
      if (result < 0 && errno == EINTR)
        continue;
#endif
      return result > 0;
    }
  };

  std::array<u8, 5> message{};
  for (std::size_t received = 0; received < message.size();)
  {
    if (!wait_for_socket(false))
      return "The server did not send a protocol version in time. Update the client and server.";
    const int count = recv(socket, reinterpret_cast<char*>(message.data() + received),
                           static_cast<int>(message.size() - received), 0);
    if (count <= 0)
      return "The server closed the connection before completing the protocol version exchange.";
    received += count;
    if (message[0] != VERSION_MESSAGE)
      return "The server does not support protocol version checking. Update the server.";
  }

  const u32 server_version = (static_cast<u32>(message[1]) << 24) |
                             (static_cast<u32>(message[2]) << 16) |
                             (static_cast<u32>(message[3]) << 8) | message[4];
  message = {VERSION_MESSAGE, static_cast<u8>(PROTOCOL_VERSION >> 24),
             static_cast<u8>(PROTOCOL_VERSION >> 16), static_cast<u8>(PROTOCOL_VERSION >> 8),
             static_cast<u8>(PROTOCOL_VERSION)};

  // A peer can close during negotiation; report the failure instead of raising SIGPIPE.
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags = MSG_NOSIGNAL;
#elif defined(SO_NOSIGPIPE)
  const int no_sigpipe = 1;
  setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
  for (std::size_t sent = 0; sent < message.size();)
  {
    if (!wait_for_socket(true))
      return "Timed out sending the client protocol version.";
    const int count = send(socket, reinterpret_cast<const char*>(message.data() + sent),
                           static_cast<int>(message.size() - sent), flags);
    if (count <= 0)
      return "Unable to send the client protocol version.";
    sent += count;
  }

  if (server_version != PROTOCOL_VERSION)
  {
    return fmt::format("Protocol version mismatch: client {}, server {}. "
                       "Update the client and server to compatible versions.",
                       PROTOCOL_VERSION, server_version);
  }
  return {};
}
}  // namespace Falconnect
