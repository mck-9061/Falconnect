#pragma once

#include <optional>
#include <span>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/Falconnect/PacketType.h"

namespace Falconnect
{
struct CourseSettings
{
  u8 track_id = 1;
  u8 opponent_count = 29;
  u8 lap_count = 3;
  u8 boost_lap = 1;
  std::string track_name = "Track 1";

  // Matches CourseMessage.java: type, track, opponents, laps, boost lap, UTF-8 byte
  // length, name. Remaining bytes in the fixed-size TCP frame are zero padding.
  static std::optional<CourseSettings> Decode(std::span<const u8> packet)
  {
    if (packet.size() < 3 || packet[0] != static_cast<u8>(FromServerPacketType::COURSE) ||
        packet[2] >= 30)
      return std::nullopt;

    CourseSettings settings;
    settings.track_id = packet[1];
    settings.opponent_count = packet[2];
    settings.track_name = "Track " + std::to_string(settings.track_id);
    if (packet.size() == 3)
      return settings;
    if (packet.size() < 6)
      return std::nullopt;

    if (packet[3] == 0 && packet[4] == 0 && packet[5] == 0)
      return settings;
    if (packet[3] == 0 || packet[4] > packet[3] ||
        packet.size() - 6 < packet[5])
      return std::nullopt;

    settings.lap_count = packet[3];
    settings.boost_lap = packet[4];
    if (packet[5] != 0)
      settings.track_name.assign(reinterpret_cast<const char*>(packet.data() + 6), packet[5]);
    return settings;
  }
};
}  // namespace Falconnect
