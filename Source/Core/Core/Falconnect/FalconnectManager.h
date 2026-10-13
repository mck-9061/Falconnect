#ifndef DOLPHIN_EMU_FALCONNECTMANAGER_H
#define DOLPHIN_EMU_FALCONNECTMANAGER_H
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <utility>

#include "GXMemoryPatcher.h"
#include "Core/Core.h"

enum class GameState : int {
    NOT_RUNNING,
    WAITING_FOR_REFERENCE,
    NOT_IN_PRACTICE,
    IN_PRACTICE,
    FAILED_TO_CONNECT,
    SETTINGS_SELECTED,
    READY_TO_LOAD,
    RACE_LOADED,
    RACE_READY,
    RACE_GRIDDED,
    RACING
};

class FalconnectManager {
public:
    explicit FalconnectManager();

    static FalconnectManager* instance;
    void Update();

    GXMemoryPatcher* patcher;

    u8 racerIDs[256];
    GameState currentState;
    std::atomic<bool> shouldStart{false};
    bool shouldReset = false;

    RacerMemoryBlock* lastWrittenBlocks[29];

private:
    void log(const std::string& message);
    std::string lastLogged;

    u8 readCounter = 0;
    u8 frameCount = 0;
    u8 displayedReadyPlayerCount = 0;
    u8 displayedTotalPlayerCount = 0;
    using FrameDifferenceSample = std::pair<std::chrono::steady_clock::time_point, s64>;
    static constexpr auto FRAME_DIFFERENCE_WINDOW = std::chrono::seconds(5);
    // The last accepted remote offset remains in use while a received snapshot's frame delay
    // differs from the rolling five-second mode for that racer.
    std::array<std::deque<FrameDifferenceSample>, 30> m_remote_frame_differences{};
    std::array<std::unordered_map<s64, std::size_t>, 30> m_remote_frame_difference_counts{};
    std::array<s64, 30> m_modal_frame_differences{};
    std::array<bool, 30> m_has_modal_frame_difference{};
    std::array<std::array<float, 3>, 30> m_latest_remote_position_offsets{};
    std::array<bool, 30> m_has_remote_position_offset{};
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
