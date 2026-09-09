#include "FalconnectManager.h"

#include <chrono>
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

void FalconnectManager::Update() {
    if (const auto& system = Core::System::GetInstance(); system.GetCPU().GetState() != CPU::State::Running) {
        log("Not running!");
        currentState = GameState::NOT_RUNNING;

        if (FalconnectSocketManager::instance != nullptr) {
            FalconnectSocketManager::instance->shouldDisconnect = true;
            shouldReset = true;
            FalconnectSocketManager::instance = nullptr;
        }

        return;
    }

    if (patcher == nullptr) {
        patcher = new GXMemoryPatcher();
    }

    if (!patcher->isReady) {
        patcher->Initialise();
        if (!patcher->isReady) return;
    }

    // Patcher ready; wait for practice mode

    if (shouldReset) {
        log("Resetting...");
        shouldReset = false;

        patcher->ResetToTitle();
        currentState = GameState::FAILED_TO_CONNECT;

        FalconnectSocketManager::instance = nullptr;
        delete patcher;
        patcher = nullptr;

        return;
    }

    if (!(
            currentState == GameState::RACE_LOADED ||
            currentState == GameState::RACE_GRIDDED ||
            currentState == GameState::RACING
            )) {
      if (const u16 mode = patcher->memoryReader->ReadGameMode(); mode != 3)
      {
        log("Not in Practice mode!");
        currentState = GameState::NOT_IN_PRACTICE;
          if (FalconnectSocketManager::instance != nullptr) FalconnectSocketManager::instance->shouldDisconnect = true;
        return;
      }

      if (currentState == GameState::NOT_IN_PRACTICE || currentState == GameState::NOT_RUNNING)
        currentState = GameState::IN_PRACTICE;

      if (currentState == GameState::FAILED_TO_CONNECT) return;

      if (currentState == GameState::IN_PRACTICE &&
          FalconnectSocketManager::instance != nullptr &&
          FalconnectSocketManager::instance->hasProperlyConnected &&
          (displayedReadyPlayerCount != FalconnectSocketManager::instance->readyPlayerCount ||
           displayedTotalPlayerCount != FalconnectSocketManager::instance->totalPlayerCount)) {
          patcher->SetPlayersReadyText(FalconnectSocketManager::instance->readyPlayerCount,
                                       FalconnectSocketManager::instance->totalPlayerCount);
          displayedReadyPlayerCount = FalconnectSocketManager::instance->readyPlayerCount;
          displayedTotalPlayerCount = FalconnectSocketManager::instance->totalPlayerCount;
      }

      if (!patcher->memoryReader->ReadSettingsSelectedFlag())
      {
        log("Waiting for all settings to be chosen...");

          // Connect to server
          if (FalconnectSocketManager::instance == nullptr || FalconnectSocketManager::instance->isError || !FalconnectSocketManager::instance->hasProperlyConnected) {
            patcher->SetPracticeModeText("Connecting to Falconnect server...");
            patcher->FullyDisableMenuControl(); // broken lol (only disables input in machine settings and options)

              delete FalconnectSocketManager::instance;

              FalconnectSocketManager::instance = new FalconnectSocketManager();

              FalconnectSocketManager::instance->name = patcher->memoryReader->ReadName();

              std::thread socketThread(&FalconnectSocketManager::SocketThread, FalconnectSocketManager::instance);

              socketThread.detach();

              INFO_LOG_FMT(FALCONNECT, "Socket thread created and listening");

              while (!(FalconnectSocketManager::instance->hasConnected || FalconnectSocketManager::instance->isError)) {
                  std::this_thread::sleep_for(std::chrono::seconds(1));
              }

              for (int i = 1; i <= 5; i++) {
                  if (FalconnectSocketManager::instance->hasProperlyConnected || FalconnectSocketManager::instance->isError) break;
                  std::this_thread::sleep_for(std::chrono::seconds(1));
              }

              patcher->ReEnableMenuControl();

              if (FalconnectSocketManager::instance->isError || !FalconnectSocketManager::instance->hasProperlyConnected) {
                  patcher->SetPracticeModeText("Failed to connect!");
                  currentState = GameState::FAILED_TO_CONNECT;

                  if (!FalconnectSocketManager::instance->hasProperlyConnected) {
                      FalconnectSocketManager::instance->isError = true;
                      FalconnectSocketManager::instance->shouldDisconnect = true;
                  }

                  SuccessAlertFmt("Failed to connect to Falconnect server!");
                  shouldReset = true;

                  return;
              }

              patcher->SetPracticeModeText("Vote for a track");
          }

        patcher->SetPracticeModeText("Vote for a track");
        currentState = GameState::IN_PRACTICE;
        return;
      }

      if (currentState == GameState::IN_PRACTICE)
        currentState = GameState::SETTINGS_SELECTED;
    }

    if (currentState == GameState::SETTINGS_SELECTED) {
        log("Setting up...");

        // Disable menu control
        patcher->DisableOptionsMenuControl();

        // Disable AI control
        patcher->DisableAIControl();

        // Disable countdown
        patcher->DisableCountdown();

        // Set up waiting text
        patcher->InitialiseText();
        patcher->SetBoostLap(1);

        patcher->SetDefaultRaceSettings();
        patcher->EnablePositionAnnouncementsInPractice();

        patcher->SetGrid();

        // Get our racer ID
        FalconnectSocketManager::instance->racerId = patcher->memoryReader->ReadSelectedRacerID();
        FalconnectSocketManager::instance->selectedCourse = patcher->memoryReader->ReadSelectedCourse();
        FalconnectSocketManager::instance->SetSelectedCustomMachineData(
            patcher->memoryReader->ReadSelectedCustomMachine());
        if (FalconnectSocketManager::instance->UsesSelectedCustomMachine()) {
            constexpr u8 custom_machine_racer_id = 0x32;
            FalconnectSocketManager::instance->racerId = custom_machine_racer_id;
            patcher->SetSelectedRacerId(custom_machine_racer_id);
        }

        currentState = GameState::READY_TO_LOAD;
        if (FalconnectSocketManager::instance != nullptr) FalconnectSocketManager::instance->canLoad = true;
        return;
    }

    // Initial setup done, wait for the race to start
    if (shouldStart) {
        shouldStart = false;
        currentState = GameState::RACE_LOADED;

        patcher->SetRenderedText("Falconnect | Synchronising...");

        // Set racer IDs
        //patcher->SetOpponentRacerId(racerIDs[1]);
        //u8 racerIds[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29};
        //patcher->SetDefaultRaceSettings();
        patcher->SetOpponentRacerIds(racerIDs);
        patcher->SetCourse(FalconnectSocketManager::instance->usedCourseId);
        patcher->SetCpuCount(FalconnectSocketManager::instance->cpuCount);
        patcher->SetGrid();
        const std::vector<u8> custom_machine_data =
            FalconnectSocketManager::instance->GetCustomMachineDataForRace();
        if (!custom_machine_data.empty()) {
            patcher->SetCustomMachineData(custom_machine_data);
            patcher->SetupCustomMachines();
        }
        //patcher->StopPhysicsOnReceivedMachines();

        patcher->StartRaceFromPracticeOptions();
    }

    // Wait for the machine to grid, then let the socket manager know we've gridded
    if (currentState == GameState::RACE_LOADED) {
        if (patcher->memoryReader->HasGridded()) {
            INFO_LOG_FMT(FALCONNECT, "Sending initial frames");

            const auto cpu_racer_indices = FalconnectSocketManager::instance->GetCpuRacerIndices();

            for (std::size_t i = 0; i < cpu_racer_indices.size(); i++) {
                const u8 cpuRacer = cpu_racer_indices[i];
                FalconnectSocketManager::instance->SendFrame(patcher->memoryReader->ReadRacerData(cpuRacer), i + 1);
                INFO_LOG_FMT(FALCONNECT, "Sent frame {}", i);
            }

            FalconnectSocketManager::instance->SendFrame(patcher->memoryReader->ReadRacerData(0), 0);
            INFO_LOG_FMT(FALCONNECT, "Sent all frames!");

            currentState = GameState::RACE_GRIDDED;
            FalconnectSocketManager::instance->hasGridded = true;
            for (const u8 cpuRacer : cpu_racer_indices)
                patcher->EnableAIControlFor(cpuRacer, 1);

            //patcher->EnableAIControlFor(0, 1);
        }
    }

    // Wait for the signal to start
    if (currentState == GameState::RACE_GRIDDED) {
        if (FalconnectSocketManager::instance->start) {
            INFO_LOG_FMT(FALCONNECT, "Starting");
            patcher->StartCountdown();
            patcher->InitialiseNameLabels();
            INFO_LOG_FMT(FALCONNECT, "Countdown started");
            currentState = GameState::RACING;
        }
    }

    if (currentState == GameState::RACING) {
        //INFO_LOG_FMT(FALCONNECT, "Sending frame");
        // Process operation queue and keep frame to send updated
        const auto cpu_racer_indices = FalconnectSocketManager::instance->GetCpuRacerIndices();
        for (const u8 cpuRacer : cpu_racer_indices)
            patcher->EnableAIControlFor(cpuRacer, 1);

        //patcher->EnableAIControlFor(0, 1);
         FalconnectSocketManager::instance->SendFrame(patcher->memoryReader->ReadRacerData(0), 0);

         for (std::size_t i = 0; i < cpu_racer_indices.size(); i++) {
             //INFO_LOG_FMT(FALCONNECT, "Reading racer at index {}", FalconnectSocketManager::instance->cpuStartIndex + i);
             const u8 cpuRacer = cpu_racer_indices[i];
             FalconnectSocketManager::instance->SendFrame(patcher->memoryReader->ReadRacerData(cpuRacer), i + 1);
         }
        //INFO_LOG_FMT(FALCONNECT, "Frame sent");

        u16 ping = FalconnectSocketManager::instance->ping - 4;
        if (ping > 1000) ping = 1; // overflow
        const u16 packetRate = static_cast<u16>(1.0 / (static_cast<double>(FalconnectSocketManager::instance->ping) / 1000.0));

        patcher->SetRenderedText("Falconnect | Ping: " + std::to_string(ping) + "ms | PR: " + std::to_string(packetRate) + "p/s");

        patcher->ConstrainMenu();
        //INFO_LOG_FMT(FALCONNECT, "Menu constrained");

        const auto data_wait_started = std::chrono::steady_clock::now();
        while (!FalconnectSocketManager::instance->hasReceived &&
               !FalconnectSocketManager::instance->shouldDisconnect) {
            if (std::chrono::steady_clock::now() - data_wait_started >= std::chrono::seconds(5)) {
                INFO_LOG_FMT(FALCONNECT, "No race data received from the server for 5 seconds");
                FalconnectSocketManager::instance->HandleConnectionLost();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (FalconnectSocketManager::instance->shouldDisconnect) return;
        FalconnectSocketManager::instance->hasReceived = false;
        FalconnectSocketManager::instance->shouldSendData = true;

        frameCount++;
        for (int i = 0; i < 30; i++) {
            //INFO_LOG_FMT(FALCONNECT, "SET_RACER_BLOCK");
            if (const auto racerNum = FalconnectSocketManager::instance->usedIndices[i]; racerNum != 0) {
                const auto racerBlock = FalconnectSocketManager::instance->allBlocks[i];

                if (FalconnectSocketManager::instance->updated[i]) {
                    patcher->SetRacerData(racerNum, *racerBlock, frameCount >= 1);
                    lastWrittenBlocks[racerNum - 1] = racerBlock;
                    //INFO_LOG_FMT(FALCONNECT, "Racer data set");
                } else {
                    patcher->SetRacerData(racerNum, *racerBlock, false);
                    lastWrittenBlocks[racerNum - 1] = racerBlock;
                    //INFO_LOG_FMT(FALCONNECT, "Racer data set");
                }
            }
        }
        if (frameCount >= 1) frameCount = 0;

        // Check if we've exited the race to the menu
        if (!patcher->memoryReader->ReadIsInRace()) {
            readCounter++;

            if (readCounter > 3) {
                INFO_LOG_FMT(FALCONNECT, "Exited race!");
                FalconnectSocketManager::instance->exited = true;

                // Check that we're still in practice mode; if not, disconnect
                u16 mode = patcher->memoryReader->ReadGameMode();
                for (int i = 0; i < 15; i++) mode = patcher->memoryReader->ReadGameMode();

                if (mode != 3) {
                    SuccessAlertFmt("You disconnected from the race.");
                    FalconnectSocketManager::instance->shouldDisconnect = true;
                    currentState = GameState::NOT_IN_PRACTICE;
                    shouldReset = true;
                } else {
                    currentState = GameState::IN_PRACTICE;
                }
            }
        } else {
            readCounter = 0;
        }
    }
}
