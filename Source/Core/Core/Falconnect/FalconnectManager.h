#ifndef DOLPHIN_EMU_FALCONNECTMANAGER_H
#define DOLPHIN_EMU_FALCONNECTMANAGER_H
#include <array>
#include <atomic>

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
    static constexpr std::size_t REMOTE_POSITION_OFFSET_HISTORY_SIZE = 4;
    // Each remote racer keeps four received positions relative to our local racer. Averaging
    // those samples avoids applying every network position as an abrupt correction.
    std::array<std::array<std::array<float, 3>, REMOTE_POSITION_OFFSET_HISTORY_SIZE>, 30>
        m_remote_position_offset_history{};
    std::array<bool, 30> m_has_remote_position_offset{};
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
