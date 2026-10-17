#ifndef DOLPHIN_EMU_FALCONNECTMANAGER_H
#define DOLPHIN_EMU_FALCONNECTMANAGER_H
#include <array>
#include <atomic>
#include <chrono>
#include <utility>

#include "GXMemoryPatcher.h"
#include "Core/Falconnect/RacerInterpolator.h"
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

    bool m_force_start_pending = false;
    std::chrono::steady_clock::time_point m_force_start_at{};
    std::chrono::steady_clock::time_point m_force_start_course_write_until{};
    u8 m_force_start_course_id = 1;
    u8 readCounter = 0;
    u8 frameCount = 0;
    u8 displayedReadyPlayerCount = 0;
    u8 displayedTotalPlayerCount = 0;
    u16 displayedForceRaceStartCountdownSeconds = 0;
    std::array<Falconnect::RacerInterpolator, 30> m_racer_interpolators;
    u32 m_last_local_race_frame_count = 0;
    std::chrono::steady_clock::time_point m_local_race_frame_last_advanced{};
    bool m_local_race_frame_count_started = false;
    bool m_local_racer_ai_enabled = false;
    std::array<u32, 30> m_last_human_race_frame_counts{};
    std::array<std::chrono::steady_clock::time_point, 30>
        m_human_race_frame_last_advanced{};
    // A disconnected player is removed from the server's full-data packet. Keep the last
    // snapshot in which each remote human slot was present so that disappearance cannot hold the
    // post-race menu closed forever.
    std::array<std::chrono::steady_clock::time_point, 30>
        m_human_racer_last_received{};
    std::array<bool, 30> m_human_race_frame_count_started{};
    std::array<bool, 30> m_human_racers_finished{};
    std::chrono::steady_clock::time_point m_human_finish_tracking_started_at{};
    bool m_all_human_racers_finished = false;
    std::chrono::steady_clock::time_point m_all_human_racers_finished_at{};
    bool m_menu_should_reopen_after_finish = false;
    bool m_menu_opened_after_finish = false;
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
