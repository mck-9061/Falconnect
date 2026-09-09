#ifndef DOLPHIN_EMU_FALCONNECTMANAGER_H
#define DOLPHIN_EMU_FALCONNECTMANAGER_H
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
    bool shouldStart;
    bool shouldReset = false;

    RacerMemoryBlock* lastWrittenBlocks[29];

private:
    void log(const std::string& message);
    std::string lastLogged;

    u8 readCounter = 0;
    u8 frameCount = 0;
    u8 displayedReadyPlayerCount = 0;
    u8 displayedTotalPlayerCount = 0;
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
