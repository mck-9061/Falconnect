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
  bool HasStarted() const { return m_started; }

  // Called at frame end only when Step did not write a position. Keep the GX position for
  // this expected frame, rather than stretching a later interpolation back to stale wire data.
  void RecordSimulatedPosition(const std::array<u32, 3>& center)
  {
    if (m_started)
      m_simulated_anchor = SimulatedAnchor{m_expected_frame, center};
  }

  // interpolationFrameCount is an independently maintained, sender-local update counter.  It is
  // relayed with each pose and is deliberately separate from the server-owned race clock.
  std::optional<Update> Step(const RacerMemoryBlock* incoming, u8 owner = 0)
  {
    if (incoming && (!m_started || owner != m_owner))
    {
      Reset();
      m_started = true;
      m_owner = owner;
      m_expected_frame = incoming->interpolationFrameCount;
      m_samples.push_back(*incoming);
      return Direct(*incoming);
    }
    if (!m_started)
      return std::nullopt;

    m_expected_frame += 1.0;
    if (incoming && incoming->interpolationFrameCount < m_samples.back().interpolationFrameCount)
      incoming = nullptr;  // Ignore regressing timestamps, but keep playing buffered samples.
    if (incoming)
    {
      const u32 frame = incoming->interpolationFrameCount;
      const u32 latest_frame = m_samples.back().interpolationFrameCount;
      if (frame == latest_frame)
      {
        m_samples.back() = *incoming;
        // Countdown and finished racers can change position with a stopped timer.
        if (m_samples.size() == 1)
        {
          m_expected_frame = frame;
          m_simulated_anchor.reset();
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
    bool crossed_endpoint = false;
    while (m_samples.size() > 1 && m_samples[1].interpolationFrameCount <= m_expected_frame)
    {
      m_samples.pop_front();
      crossed_endpoint = true;
    }
    if (m_samples.size() == 1)
    {
      const auto& latest = m_samples.front();
      if (incoming || crossed_endpoint || m_expected_frame == latest.interpolationFrameCount)
      {
        // The receive stream can stay behind our predicted frame after one missed update.
        // Recover from its newest authoritative pose instead of suppressing full writes
        // forever (the patcher's full flag gates rotation and velocity as well as position).
        // Also commit a buffered endpoint when fractional catch-up steps past it.
        m_expected_frame = latest.interpolationFrameCount;
        m_simulated_anchor.reset();
        m_ahead_samples = 0;
        return Direct(latest);
      }
      return std::nullopt;
    }

    const auto& left = m_samples[0];
    const auto& right = m_samples[1];
    double left_frame = left.interpolationFrameCount;
    std::array<u32, 3> left_center = {
        left.centerPosition[0], left.centerPosition[1], left.centerPosition[2]};
    if (m_simulated_anchor && m_simulated_anchor->frame > left_frame &&
        m_simulated_anchor->frame < right.interpolationFrameCount)
    {
      left_frame = m_simulated_anchor->frame;
      left_center = m_simulated_anchor->center;
    }
    const double fraction = (m_expected_frame - left_frame) /
                            (right.interpolationFrameCount - left_frame);
    Update update{left, {}, true};
    for (std::size_t axis = 0; axis < update.center.size(); ++axis)
    {
      const float a = std::bit_cast<float>(left_center[axis]);
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
  struct SimulatedAnchor
  {
    double frame;
    std::array<u32, 3> center;
  };
  std::optional<SimulatedAnchor> m_simulated_anchor;
  std::deque<RacerMemoryBlock> m_samples;
  double m_expected_frame = 0;
  unsigned m_ahead_samples = 0;
  u8 m_owner = 0;
  bool m_started = false;
};
}  // namespace Falconnect
