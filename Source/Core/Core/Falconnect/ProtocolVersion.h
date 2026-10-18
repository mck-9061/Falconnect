#pragma once

#include <chrono>
#include <string>

#include "Common/CommonTypes.h"

namespace Falconnect
{
// Bump both this and ProtocolVersion.java when either TCP or UDP wire compatibility changes.
inline constexpr u32 PROTOCOL_VERSION = 10;
inline constexpr u8 VERSION_MESSAGE = 0x7f;

// The server speaks first. This five-byte message is independent of the normal frame size.
// Returns an empty string on success, otherwise a user-facing connection failure reason.
std::string ExchangeProtocolVersion(int socket,
                                    std::chrono::milliseconds timeout = std::chrono::seconds(5));
}  // namespace Falconnect
