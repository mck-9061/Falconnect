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

void FalconnectManager::Update(const Core::CPUThreadGuard& guard) {
    if (const auto& system = Core::System::GetInstance(); system.GetCPU().GetState() != CPU::State::Running) {
        INFO_LOG_FMT(FALCONNECT, "Not running!");
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
    INFO_LOG_FMT(FALCONNECT, "Current speed: {}", stream2.str());

    if (const u16 mode = patcher->memoryReader->ReadGameMode(); mode != 3) {
        INFO_LOG_FMT(FALCONNECT, "Not in Practice mode!");
        currentState = GameState::NOT_IN_PRACTICE;
        return;
    }

    if (currentState == GameState::NOT_IN_PRACTICE || currentState == GameState::NOT_RUNNING) currentState = GameState::IN_PRACTICE;

    if (!patcher->memoryReader->ReadSettingsSelectedFlag()) {
        INFO_LOG_FMT(FALCONNECT, "Waiting for all settings to be chosen...");
        currentState = GameState::IN_PRACTICE;
        return;
    }

    if (currentState == GameState::IN_PRACTICE) currentState = GameState::SETTINGS_SELECTED;

    if (currentState == GameState::SETTINGS_SELECTED) {
        INFO_LOG_FMT(FALCONNECT, "Setting up...");

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
