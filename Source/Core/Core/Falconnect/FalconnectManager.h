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
    void Update(const Core::CPUThreadGuard& guard);

    GXMemoryPatcher* patcher;

    u8 racerIDs[256];
    GameState currentState;
    bool shouldStart;

private:
    void log(const std::string& message);
    std::string lastLogged;

    u8 readCounter = 0;
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
