#ifndef DOLPHIN_EMU_FALCONNECTMANAGER_H
#define DOLPHIN_EMU_FALCONNECTMANAGER_H
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
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
    // Socket callbacks run on a separate thread.  Queue their error for Update(), which owns
    // game-memory reset and UI notification.
    void RequestResetWithError(std::string message);

    GXMemoryPatcher* patcher;

    u8 racerIDs[256];
    GameState currentState;
    std::atomic<bool> shouldStart{false};
    std::atomic<bool> shouldReset{false};

    RacerMemoryBlock* lastWrittenBlocks[29];

private:
    std::mutex m_reset_error_mutex;
    std::optional<std::string> m_reset_error;
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
    u8 displayedLobbyPhase = 0xff;
    std::string displayedTrackName;
    bool m_track_vote_submitted = false;
    bool m_seen_track_select = false;
    std::array<Falconnect::RacerInterpolator, 30> m_racer_interpolators;
    bool m_local_racer_ai_enabled = false;
    std::array<u32, 30> m_last_lap_counts{};
    std::array<u64, 30> m_completed_lap_millis{};
    std::array<bool, 30> m_human_racers_finished{};
    // Server slots which disappear after sending race data belong to departed players, not
    // racers which still need to cross the finish line.
    std::array<std::chrono::steady_clock::time_point, 30> m_human_racer_last_received{};
    std::chrono::steady_clock::time_point m_human_finish_tracking_started_at{};
    bool m_all_human_racers_finished = false;
    std::chrono::steady_clock::time_point m_all_human_racers_finished_at{};
    bool m_menu_should_reopen_after_finish = false;
    bool m_menu_opened_after_finish = false;
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
