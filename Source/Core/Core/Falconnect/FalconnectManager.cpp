#include "FalconnectManager.h"

#include <iostream>
#include <thread>

#include "FalconnectSocketManager.h"
#include "PowerPCScripts.h"
#include "Core/System.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PowerPC.h"

FalconnectManager* FalconnectManager::instance = nullptr;

FalconnectManager::FalconnectManager() {
    patcher = nullptr;
    currentState = GameState::NOT_RUNNING;
    instance = this;
    shouldStart = false;
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

    if (!(
            currentState == GameState::RACE_LOADED ||
            currentState == GameState::RACE_GRIDDED ||
            currentState == GameState::RACING
            )) {
      if (const u16 mode = patcher->memoryReader->ReadGameMode(); mode != 3)
      {
        log("Not in Practice mode!");
        currentState = GameState::NOT_IN_PRACTICE;
        return;
      }

      if (currentState == GameState::NOT_IN_PRACTICE || currentState == GameState::NOT_RUNNING)
        currentState = GameState::IN_PRACTICE;

      if (!patcher->memoryReader->ReadSettingsSelectedFlag())
      {
        log("Waiting for all settings to be chosen...");
        currentState = GameState::IN_PRACTICE;
        return;
      }

      if (currentState == GameState::IN_PRACTICE)
        currentState = GameState::SETTINGS_SELECTED;
    }

    if (currentState == GameState::SETTINGS_SELECTED) {
        log("Setting up...");

        // Disable menu control
        patcher->DisableMenuControl();

        // Disable AI control
        patcher->DisableAIControl();

        // Disable countdown
        patcher->DisableCountdown();

        // Set up waiting text
        patcher->InitialiseText();
        patcher->SetBoostLap(2);

        patcher->SetDefaultRaceSettings();

        // Get our racer ID
        racerIDs[0] = patcher->memoryReader->ReadSelectedRacerID();

        currentState = GameState::READY_TO_LOAD;
        return;
    }

    // Initial setup done, wait for the race to start
    if (shouldStart) {
        shouldStart = false;
        currentState = GameState::RACE_LOADED;

        patcher->SetRenderedText("Falconnect | Synchronising...");


        // Set racer IDs
        patcher->SetOpponentRacerId(racerIDs[1]);

        patcher->StartRaceFromPracticeOptions();
    }

    // Wait for the machine to grid, then let the socket manager know we've gridded
    if (currentState == GameState::RACE_LOADED) {
        if (patcher->memoryReader->HasGridded()) {
            FalconnectSocketManager::instance->SendFrame(patcher->memoryReader->ReadRacerData(0));
            currentState = GameState::RACE_GRIDDED;
            FalconnectSocketManager::instance->hasGridded = true;
        }
    }

    // Wait for the signal to start
    if (currentState == GameState::RACE_GRIDDED) {
        if (FalconnectSocketManager::instance->start) {
            INFO_LOG_FMT(FALCONNECT, "Starting");
            patcher->StartCountdown();
            INFO_LOG_FMT(FALCONNECT, "Countdown started");
            currentState = GameState::RACING;
        }
    }

    if (currentState == GameState::RACING) {
        INFO_LOG_FMT(FALCONNECT, "Sending frame");
        // Process operation queue and keep frame to send updated
        FalconnectSocketManager::instance->SendFrame(patcher->memoryReader->ReadRacerData(0));
        INFO_LOG_FMT(FALCONNECT, "Frame sent");

        if (const u32 queueLength = static_cast<u32>(FalconnectSocketManager::instance->operationQueue.size()); queueLength != 0)
        {
          patcher->SetRenderedText("Falconnect | Ping: " + std::to_string(FalconnectSocketManager::instance->ping) + "ms");

          const OperationType operation = FalconnectSocketManager::instance->operationQueue.front();
          FalconnectSocketManager::instance->operationQueue.pop();


          switch (operation)
          {
            case (OperationType::SET_RACER_BLOCK):
            {
              const auto racerBlock = get<RacerMemoryBlock>(
                  FalconnectSocketManager::instance->operationArgumentsQueue.front());
              FalconnectSocketManager::instance->operationArgumentsQueue.pop();

              patcher->SetRacerData(1, racerBlock);

              break;
            }

            default:
              break;
          }
       
        }
    }
}
