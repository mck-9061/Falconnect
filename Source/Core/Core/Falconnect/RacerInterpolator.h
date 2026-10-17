#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <deque>
#include <optional>

#include "Core/Falconnect/RacerMemoryBlock.h"

namespace Falconnect
{
// CPU-thread only. Positions are float bit patterns on the wire, not integer coordinates.
class RacerInterpolator
{
public:
  struct Update
  {
    RacerMemoryBlock block;
    std::array<u32, 3> center;
    bool write_position;
  };

  void Reset() { *this = {}; }
  double ExpectedFrame() const { return m_expected_frame; }

  std::optional<Update> Step(const RacerMemoryBlock* incoming, u8 owner = 0)
  {
    if (incoming && (!m_started || owner != m_owner))
    {
      Reset();
      m_started = true;
      m_owner = owner;
      m_expected_frame = incoming->raceTimeFrameCount;
      m_samples.push_back(*incoming);
      return Direct(*incoming);
    }
    if (!m_started)
      return std::nullopt;

    m_expected_frame += 1.0;
    if (incoming && incoming->raceTimeFrameCount < m_samples.back().raceTimeFrameCount)
      incoming = nullptr;  // Ignore regressing timestamps, but keep playing buffered samples.
    if (incoming)
    {
      const u32 frame = incoming->raceTimeFrameCount;
      const u32 latest_frame = m_samples.back().raceTimeFrameCount;
      if (frame == latest_frame)
      {
        m_samples.back() = *incoming;
        // Countdown and finished racers can change position with a stopped timer.
        if (m_samples.size() == 1)
        {
          m_expected_frame = frame;
          m_ahead_samples = 0;
          return Direct(*incoming);
        }
      }
      else
      {
        m_samples.push_back(*incoming);
        // Bound history during extreme sustained lead without losing the current bracket or
        // newest endpoint. Only distant intermediate samples are coalesced.
        if (m_samples.size() > MAX_SAMPLES)
          m_samples.erase(m_samples.end() - 2);
        const double lead = frame - m_expected_frame;
        m_ahead_samples = lead >= 1.0 ? std::min(m_ahead_samples + 1, CATCH_UP_SAMPLES) : 0;
        if (m_ahead_samples == CATCH_UP_SAMPLES)
          m_expected_frame += std::min(MAX_EXTRA_STEP, lead * 0.05);
      }
    }

    // Keep the closest real sample on either side of the expected frame.
    while (m_samples.size() > 1 && m_samples[1].raceTimeFrameCount <= m_expected_frame)
      m_samples.pop_front();
    if (m_samples.size() == 1)
    {
      const auto& latest = m_samples.front();
      if (m_expected_frame == latest.raceTimeFrameCount)
        return Direct(latest);
      // Late packets still carry current inputs/state/timing, but cannot supply a future
      // endpoint. Apply those fields once without rewinding position or extrapolating.
      if (incoming)
        return Update{*incoming, {}, false};
      return std::nullopt;
    }

    const auto& left = m_samples[0];
    const auto& right = m_samples[1];
    const double fraction = (m_expected_frame - left.raceTimeFrameCount) /
                            (right.raceTimeFrameCount - left.raceTimeFrameCount);
    Update update{left, {}, true};
    for (std::size_t axis = 0; axis < update.center.size(); ++axis)
    {
      const float a = std::bit_cast<float>(left.centerPosition[axis]);
      const float b = std::bit_cast<float>(right.centerPosition[axis]);
      if (!std::isfinite(a) || !std::isfinite(b))
        return std::nullopt;
      update.center[axis] = std::bit_cast<u32>(static_cast<float>(std::lerp(
          static_cast<double>(a), static_cast<double>(b), fraction)));
    }
    // Do not apply future discrete inputs/lap state early. The preceding real sample supplies
    // other fields until the next endpoint is reached; only center position is interpolated.
    return update;
  }

private:
  static Update Direct(const RacerMemoryBlock& block)
  {
    return {block, {block.centerPosition[0], block.centerPosition[1], block.centerPosition[2]}, true};
  }

  static constexpr std::size_t MAX_SAMPLES = 128;
  static constexpr unsigned CATCH_UP_SAMPLES = 8;
  static constexpr double MAX_EXTRA_STEP = 0.25;
  std::deque<RacerMemoryBlock> m_samples;
  double m_expected_frame = 0;
  unsigned m_ahead_samples = 0;
  u8 m_owner = 0;
  bool m_started = false;
};
}  // namespace Falconnect
