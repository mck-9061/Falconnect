#ifndef DOLPHIN_EMU_FALCONNECTMANAGER_H
#define DOLPHIN_EMU_FALCONNECTMANAGER_H
#include "GXMemoryPatcher.h"
#include "Core/Core.h"

enum class GameState : int {
    NOT_RUNNING,
    WAITING_FOR_REFERENCE,
    NOT_IN_PRACTICE,
    IN_PRACTICE,
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

private:
    GXMemoryPatcher* patcher;
    GameState currentState;

    void log(const std::string& message);
    std::string lastLogged;
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
