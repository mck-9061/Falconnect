#include "FalconnectManager.h"

#include <iostream>
#include <thread>

#include "PowerPCScripts.h"
#include "Core/System.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PowerPC.h"

FalconnectManager* FalconnectManager::instance = nullptr;

FalconnectManager::FalconnectManager() {
    patcher = nullptr;
    currentState = GameState::NOT_RUNNING;
    instance = this;
}

void FalconnectManager::log(const std::string& message) {
    if (message != lastLogged) {
        lastLogged = message;
        INFO_LOG_FMT(FALCONNECT, "{}", lastLogged);
    }
}

void FalconnectManager::Update(const Core::CPUThreadGuard& guard) {
    if (const auto& system = Core::System::GetInstance(); system.GetCPU().GetState() != CPU::State::Running) {
        log("Not running!");
        currentState = GameState::NOT_RUNNING;
        return;
    }

    if (patcher == nullptr) {
        patcher = new GXMemoryPatcher(guard);
    }

    if (!patcher->isReady) {
        patcher->Initialise();
        if (!patcher->isReady) return;
    }

    // Patcher ready; wait for practice mode

    const RacerMemoryBlock* block = patcher->memoryReader->ReadRacerData(0);
    std::stringstream stream2;
    stream2 << std::hex << block->speed;

    if (const u16 mode = patcher->memoryReader->ReadGameMode(); mode != 3) {
        log("Not in Practice mode!");
        currentState = GameState::NOT_IN_PRACTICE;
        return;
    }

    if (currentState == GameState::NOT_IN_PRACTICE || currentState == GameState::NOT_RUNNING) currentState = GameState::IN_PRACTICE;

    if (!patcher->memoryReader->ReadSettingsSelectedFlag()) {
        log("Waiting for all settings to be chosen...");
        currentState = GameState::IN_PRACTICE;
        return;
    }

    if (currentState == GameState::IN_PRACTICE) currentState = GameState::SETTINGS_SELECTED;

    if (currentState == GameState::SETTINGS_SELECTED) {
        log("Setting up...");

        // Disable menu control
        //patcher->DisableMenuControl();

        // Disable AI control

        // Disable countdown

        // Set up waiting text
        patcher->InitialiseText();
        patcher->SetBoostLap(2);

        currentState = GameState::READY_TO_LOAD;
        return;
    }

    // Initial setup done, wait for the race to start

}
