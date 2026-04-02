//
// Created by Nathaniel Carter on 31/03/2026.
//

#ifndef DOLPHIN_EMU_FALCONNECTMANAGER_H
#define DOLPHIN_EMU_FALCONNECTMANAGER_H
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
    void LoadReferencePointer(const Core::CPUThreadGuard& guard);
    u16 Read16(const Core::CPUThreadGuard& guard, u32 offset);

    u32 referencePointer;
    GameState currentState;
};



#endif //DOLPHIN_EMU_FALCONNECTMANAGER_H
