#include "FalconnectManager.h"

#include <bit>
#include <chrono>
#include <iostream>
#include <memory>
#include <utility>
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

                  const auto& connection_error = FalconnectSocketManager::instance->connectionError;
                  SuccessAlertFmt("Failed to connect to Falconnect server!{}{}",
                                  connection_error.empty() ? "" : "\n", connection_error);
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
    if (shouldStart.exchange(false)) {
        currentState = GameState::RACE_LOADED;

        patcher->SetRenderedText("Falconnect | Synchronising...");

        // Set racer IDs
        //patcher->SetOpponentRacerId(racerIDs[1]);
        //u8 racerIds[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29};
        //patcher->SetDefaultRaceSettings();
        patcher->SetOpponentRacerIds(racerIDs);
        // Apply one complete server snapshot before GX starts loading the selected course.
        const auto course = FalconnectSocketManager::instance->GetCourseSettings();
        patcher->SetCourse(course.track_id);
        patcher->SetCpuCount(course.opponent_count);
        patcher->SetLapCount(course.lap_count);
        patcher->SetBoostLap(course.boost_lap);
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
            currentState = GameState::RACE_GRIDDED;
            FalconnectSocketManager::instance->hasGridded = true;
        }
    }

    // Wait for the signal to start
    if (currentState == GameState::RACE_GRIDDED) {
        if (FalconnectSocketManager::instance->start) {
            // INFO_LOG_FMT(FALCONNECT, "Starting");
            patcher->StartCountdown();
            patcher->InitialiseNameLabels();
            // INFO_LOG_FMT(FALCONNECT, "Countdown started");
            currentState = GameState::RACING;
        }
    }

    // Update runs only from Core::OnFrameEnd. Sample every locally controlled racer exactly
    // once here, including on the grid-to-race transition, before publishing any of them.
    if (currentState == GameState::RACE_GRIDDED || currentState == GameState::RACING) {
        auto* socket_manager = FalconnectSocketManager::instance;
        const auto cpu_racer_indices = socket_manager->GetCpuRacerIndices();
        FalconnectSocketManager::OutgoingRaceData frame{};
        const auto sample_racer = [&](u8 racer) {
            const std::unique_ptr<RacerMemoryBlock> block(patcher->memoryReader->ReadRacerData(racer));
            if (block)
                frame[racer] = *block;
        };
        sample_racer(0);
        for (const u8 racer : cpu_racer_indices) {
            patcher->EnableAIControlFor(racer, 1);
            sample_racer(racer);
        }
        socket_manager->PublishFrame(std::move(frame), cpu_racer_indices);
    }

    if (currentState == GameState::RACING) {
        u16 ping = FalconnectSocketManager::instance->ping - 4;
        if (ping > 1000) ping = 1; // overflow
        const u16 packetRate = FalconnectSocketManager::instance->ping == 0 ? 0 :
            static_cast<u16>(1000 / FalconnectSocketManager::instance->ping);

        patcher->SetRenderedText("Falconnect | Ping: " + std::to_string(ping) + "ms | PR: " + std::to_string(packetRate) + "p/s");

        patcher->ConstrainMenu();
        //INFO_LOG_FMT(FALCONNECT, "Menu constrained");

        FalconnectSocketManager::ReceivedRaceData received_race_data;
        FalconnectSocketManager::instance->ConsumeReceivedRaceData(&received_race_data);
        for (const auto& received_racer : received_race_data.racers) {
            if (!received_racer.valid ||
                FalconnectSocketManager::instance->OwnsCpuRacer(received_racer.racer_index))
                continue;
            patcher->SetRacerData(received_racer.racer_index, received_racer.block,
                                  received_racer.full_updated);
        }

        // Check if we've exited the race to the menu
        if (!patcher->memoryReader->ReadIsInRace()) {
            readCounter++;

            if (readCounter > 3) {
                // INFO_LOG_FMT(FALCONNECT, "Exited race!");
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
