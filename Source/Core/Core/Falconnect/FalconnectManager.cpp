#include "FalconnectManager.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <fmt/format.h>

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
        m_force_start_pending = false;
        m_force_start_course_write_until = {};

        if (FalconnectSocketManager::instance != nullptr) {
            FalconnectSocketManager::instance->RequestDisconnect();
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
        m_force_start_pending = false;
        m_force_start_course_write_until = {};
        shouldStart = false;

        patcher->ResetToTitle();
        currentState = GameState::FAILED_TO_CONNECT;

        FalconnectSocketManager::instance = nullptr;
        delete patcher;
        patcher = nullptr;

        return;
    }

    // Consume START once so a packet arriving during this update cannot bypass setup.
    const bool server_requested_start = shouldStart.exchange(false);
    if (!server_requested_start && !(
            currentState == GameState::RACE_LOADED ||
            currentState == GameState::RACE_GRIDDED ||
            currentState == GameState::RACING
            )) {
      if (const u16 mode = patcher->memoryReader->ReadGameMode(); mode != 3)
      {
        log("Not in Practice mode!");
        currentState = GameState::NOT_IN_PRACTICE;
          if (FalconnectSocketManager::instance != nullptr)
              FalconnectSocketManager::instance->RequestDisconnect();
        return;
      }

      if (currentState == GameState::NOT_IN_PRACTICE || currentState == GameState::NOT_RUNNING)
        currentState = GameState::IN_PRACTICE;

      if (currentState == GameState::FAILED_TO_CONNECT) return;

      // A local player enters SETTINGS_SELECTED/READY_TO_LOAD immediately after readying, but
      // lobby status continues changing until the race starts. Keep the display live for every
      // non-race menu state, not only while this player remains in Practice.
      if (FalconnectSocketManager::instance != nullptr &&
          FalconnectSocketManager::instance->hasProperlyConnected &&
          (displayedReadyPlayerCount != FalconnectSocketManager::instance->readyPlayerCount ||
           displayedTotalPlayerCount != FalconnectSocketManager::instance->totalPlayerCount ||
           displayedForceRaceStartCountdownSeconds !=
               FalconnectSocketManager::instance->forceRaceStartCountdownSeconds)) {
          patcher->SetPlayersReadyText(FalconnectSocketManager::instance->readyPlayerCount,
                                       FalconnectSocketManager::instance->totalPlayerCount,
                                       FalconnectSocketManager::instance->forceRaceStartCountdownSeconds);
          displayedReadyPlayerCount = FalconnectSocketManager::instance->readyPlayerCount;
          displayedTotalPlayerCount = FalconnectSocketManager::instance->totalPlayerCount;
          displayedForceRaceStartCountdownSeconds =
              FalconnectSocketManager::instance->forceRaceStartCountdownSeconds;
      }

      if (!patcher->memoryReader->ReadSettingsSelectedFlag())
      {
        log("Waiting for all settings to be chosen...");

          // Connect to server
          if (FalconnectSocketManager::instance == nullptr || FalconnectSocketManager::instance->isError || !FalconnectSocketManager::instance->hasProperlyConnected) {
            patcher->SetPracticeModeText("Connecting to Falconnect server...");
            //patcher->FullyDisableMenuControl();

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

              //patcher->ReEnableMenuControl();
              patcher->DisableB();

              if (FalconnectSocketManager::instance->isError || !FalconnectSocketManager::instance->hasProperlyConnected) {
                  patcher->SetPracticeModeText("Failed to connect!");
                  currentState = GameState::FAILED_TO_CONNECT;

                  if (!FalconnectSocketManager::instance->hasProperlyConnected) {
                      FalconnectSocketManager::instance->isError = true;
                      FalconnectSocketManager::instance->RequestDisconnect();
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

    if (currentState == GameState::SETTINGS_SELECTED ||
        (server_requested_start && currentState != GameState::READY_TO_LOAD)) {
        log("Setting up...");

        // Disable menu control
        patcher->DisableOptionsMenuControl();

        // Disable AI control
        //patcher->DisableAIControl();

        // Disable countdown
        patcher->DisableCountdown();

        // Set up waiting text
        patcher->InitialiseText();

        patcher->SetDefaultRaceSettings();
        patcher->EnablePositionAnnouncementsInPractice();

        patcher->SetGrid();

        patcher->DisablePausing();

        if (!server_requested_start) {
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
        }

        currentState = GameState::READY_TO_LOAD;
        if (!server_requested_start) {
            if (FalconnectSocketManager::instance != nullptr) FalconnectSocketManager::instance->canLoad = true;
            return;
        }
    }

    // Initial setup done, wait for the race to start
    if (server_requested_start) {
        INFO_LOG_FMT(FALCONNECT, "LOAD player={} setup_complete=1",
                     FalconnectSocketManager::instance->playerNumber);
        currentState = GameState::RACE_LOADED;
        readCounter = 0;
        for (auto& interpolator : m_racer_interpolators)
            interpolator.Reset();
        m_last_local_race_frame_count = 0;
        m_local_race_frame_last_advanced = {};
        m_local_race_frame_count_started = false;
        m_local_racer_ai_enabled = false;
        m_last_human_race_frame_counts.fill(0);
        m_human_race_frame_last_advanced.fill(std::chrono::steady_clock::time_point{});
        m_human_racer_last_received.fill(std::chrono::steady_clock::time_point{});
        m_human_race_frame_count_started.fill(false);
        m_human_racers_finished.fill(false);
        m_human_finish_tracking_started_at = {};
        m_all_human_racers_finished = false;
        m_all_human_racers_finished_at = {};
        m_menu_should_reopen_after_finish = false;
        m_menu_opened_after_finish = false;

        patcher->SetRenderedText("Falconnect | Synchronising...");

        // Use the server's selection, including its default for players forced out of menus.
        const std::vector<u8> custom_machine_data =
            FalconnectSocketManager::instance->GetCustomMachineDataForRace();
        patcher->SetSelectedMachine(FalconnectSocketManager::instance->raceRacerId,
                                    custom_machine_data);
        // Set racer IDs
        patcher->SetOpponentRacerIds(racerIDs);

        const auto course = FalconnectSocketManager::instance->GetCourseSettings();
        m_force_start_course_id = course.track_id;
        patcher->SetCourse(course.track_id);
        patcher->SetCpuCount(course.opponent_count);
        patcher->SetLapCount(course.lap_count);
        patcher->SetBoostLap(course.boost_lap);
        patcher->SetGrid();
        patcher->DisablePausing();

        if (!custom_machine_data.empty()) {
            patcher->SetCustomMachineData(custom_machine_data);
            patcher->SetupCustomMachines();
        }

        patcher->StartRaceFromPracticeOptions();
        m_force_start_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        m_force_start_pending = true;
    }

    // Keep emulation running during the delay, and finish the fallback before reporting GRIDDED.
    if (m_force_start_pending) {
        const auto now = std::chrono::steady_clock::now();
        // A forced client may still be hovering a different course in Track Select. Keep the
        // server's course in memory on every frame leading up to the force, including the frame
        // which loads the force-start code.
        patcher->SetCourse(m_force_start_course_id);
        if (now < m_force_start_at) return;
        patcher->ForceStartRace();
        m_force_start_course_write_until = now + std::chrono::seconds(1);
        INFO_LOG_FMT(FALCONNECT, "LOAD force fallback applied; in_race={}",
                     patcher->memoryReader->ReadIsInRace());
        m_force_start_pending = false;
        return;
    }

    // Track Select can overwrite the selection while its transition code is still running.
    // Continue winning that race for one second after ForceStartRace was installed.
    if (std::chrono::steady_clock::now() < m_force_start_course_write_until)
        patcher->SetCourse(m_force_start_course_id);

    // Wait for the machine to grid, then let the socket manager know we've gridded
    if (currentState == GameState::RACE_LOADED) {
        if (patcher->memoryReader->HasGridded()) {
            // INFO_LOG_FMT(FALCONNECT, "Sending initial frames");

            const auto cpu_racer_indices = FalconnectSocketManager::instance->GetCpuRacerIndices();

            INFO_LOG_FMT(FALCONNECT, "LOAD gridded player={} owned_cpus={}",
                         FalconnectSocketManager::instance->playerNumber, cpu_racer_indices.size());
            currentState = GameState::RACE_GRIDDED;
            FalconnectSocketManager::instance->hasGridded = true;

            // testing
            //for (const u8 cpuRacer : cpu_racer_indices)
                //patcher->EnableAIControlFor(1, 29);

            //patcher->EnableAIControlFor(0, 1);
        }
    }

    // Wait for the signal to start
    if (currentState == GameState::RACE_GRIDDED) {
        if (FalconnectSocketManager::instance->start) {
            // INFO_LOG_FMT(FALCONNECT, "Starting");
            patcher->StartCountdown();
            patcher->InitialiseNameLabels();
            // INFO_LOG_FMT(FALCONNECT, "Countdown started");
            INFO_LOG_FMT(FALCONNECT, "LOAD racing player={}",
                         FalconnectSocketManager::instance->playerNumber);
            m_human_finish_tracking_started_at = std::chrono::steady_clock::now();
            currentState = GameState::RACING;
        }
    }

    if (currentState == GameState::RACING) {
        // Do not leave a pause/results menu over the race. Record that it was actually opened so
        // only those clients receive the course-select menu after the post-finish grace period.
        if (!m_menu_opened_after_finish && patcher->IsMenuOpen()) {
            patcher->CloseMenu();
            m_menu_should_reopen_after_finish = true;
        }

        //INFO_LOG_FMT(FALCONNECT, "Sending frame");
        // Update frames to send
        const auto cpu_racer_indices = FalconnectSocketManager::instance->GetCpuRacerIndices();
        // for (const u8 cpuRacer : cpu_racer_indices)
        //     patcher->EnableAIControlFor(cpuRacer, 1);

        std::vector<RacerMemoryBlock> frames;
        const auto sample_racer = [&](u8 racer) {
            const std::unique_ptr<RacerMemoryBlock> block(patcher->memoryReader->ReadRacerData(racer));
            frames.push_back(*block);
        };
        sample_racer(0);
        for (const u8 racer : cpu_racer_indices)
            sample_racer(racer);
        FalconnectSocketManager::instance->PublishRaceData(frames, cpu_racer_indices);

        const u16 ping = FalconnectSocketManager::instance->ping.load();
        const u16 packetRate = FalconnectSocketManager::instance->packetRate.load();
        const std::string ping_display = FalconnectSocketManager::instance->hasPingSample.load() ?
            std::to_string(ping) + "ms" : "--";

        patcher->ConstrainMenu();
        //INFO_LOG_FMT(FALCONNECT, "Menu constrained");

        FalconnectSocketManager::ReceivedRaceData received_race_data;
        // This runs on the emulation update path. Never stall it while the freshly-started UDP
        // worker establishes its first exchange; DataThread owns the timed failure path.
        const bool received_snapshot =
            FalconnectSocketManager::instance->ConsumeReceivedRaceData(&received_race_data);

        // Finish detection uses authoritative received timing. Each remote racer separately
        // advances its interpolation timeline once per emulated frame below.
        const u32 local_frame_count = frames.front().raceTimeFrameCount;
        const auto received_snapshot_time = std::chrono::steady_clock::now();

        // GX stops increasing the local race-time frame counter once the local racer finishes.
        // We receive server snapshots more often than GX renders frames, so only interpret it as
        // stopped after a full second without an advance. This gives the game AI control of the
        // finished local machine while the player waits for the rest of the field.
        // if (local_frame_count > m_last_local_race_frame_count) {
        //     m_last_local_race_frame_count = local_frame_count;
        //     m_local_race_frame_last_advanced = received_snapshot_time;
        //     m_local_race_frame_count_started = true;
        // } else if (m_local_race_frame_count_started && !m_local_racer_ai_enabled &&
        //            received_snapshot_time - m_local_race_frame_last_advanced >=
        //                std::chrono::seconds(1)) {
        //     patcher->EnableAIControlFor(0, 1);
        //     m_local_racer_ai_enabled = true;
        //     INFO_LOG_FMT(FALCONNECT, "Local racer frame counter stopped at {}; enabling AI control",
        //                  local_frame_count);
        // }

        const u8 local_human_racer = FalconnectSocketManager::instance->playerNumber - 1;
        const u8 human_racer_count = std::min<u8>(
            FalconnectSocketManager::instance->totalPlayerCount, 30);

        const auto update_human_finish_state =
            [&](const u8 racer_num, const u32 racer_frame_count) {
                if (racer_num >= human_racer_count) return;

                if (racer_frame_count > m_last_human_race_frame_counts[racer_num]) {
                    m_last_human_race_frame_counts[racer_num] = racer_frame_count;
                    m_human_race_frame_last_advanced[racer_num] = received_snapshot_time;
                    m_human_race_frame_count_started[racer_num] = true;
                    // A delayed fresh packet can prove that an apparent one-second stall was
                    // network delay rather than a finish. Keep the menu locked in that case.
                    if (m_human_racers_finished[racer_num] && !m_menu_opened_after_finish) {
                        m_human_racers_finished[racer_num] = false;
                        m_all_human_racers_finished = false;
                        m_all_human_racers_finished_at = {};
                    }
                    return;
                }

                if (m_human_race_frame_count_started[racer_num] &&
                    received_snapshot_time - m_human_race_frame_last_advanced[racer_num] >=
                        std::chrono::seconds(1)) {
                    m_human_racers_finished[racer_num] = true;
                }
            };

        update_human_finish_state(local_human_racer, local_frame_count);
        m_human_racer_last_received[local_human_racer] = received_snapshot_time;

        for (int i = 0; i < 30; i++) {
            //INFO_LOG_FMT(FALCONNECT, "SET_RACER_BLOCK");
            const auto& received_racer = received_race_data.racers[i];
            if (received_racer.valid) {
                const auto& racer_block = received_racer.block;

                // racer_index is remapped for this client's GX memory. The received array slot
                // remains the authoritative server player slot, so use it to track finishes.
                // The local racer is sampled directly from GX memory above; CPU slots are not
                // considered by update_human_finish_state.
                const u8 source_racer_slot = static_cast<u8>(i);
                if (source_racer_slot != local_human_racer) {
                    if (source_racer_slot < human_racer_count)
                        m_human_racer_last_received[source_racer_slot] = received_snapshot_time;
                    update_human_finish_state(source_racer_slot, racer_block.raceTimeFrameCount);
                }

            }

            const u8 racer_num = i == 0 ? local_human_racer : static_cast<u8>(i);
            auto& interpolator = m_racer_interpolators[i];
            if (i == local_human_racer ||
                FalconnectSocketManager::instance->OwnsCpuRacer(racer_num) ||
                (received_snapshot && !received_racer.valid)) {
                interpolator.Reset();
                continue;
            }

            const auto update = interpolator.Step(
                received_racer.valid && received_racer.pending ? &received_racer.block : nullptr,
                static_cast<u8>(received_racer.source_sequence >> 24));
            if (update)
                patcher->SetRacerData(racer_num, update->block, update->write_position, update->center);
        }

        // Once the server has timed out or removed a remote player, their record becomes absent
        // from each full-data snapshot. They can no longer advance their race timer, so do not
        // require a finish signal that will never arrive before reopening the menu.
        for (u8 racer_num = 0; racer_num < human_racer_count; racer_num++) {
            if (racer_num == local_human_racer)
                continue;

            const auto last_received = m_human_racer_last_received[racer_num];
            const bool never_received_after_start =
                last_received == std::chrono::steady_clock::time_point{} &&
                received_snapshot_time - m_human_finish_tracking_started_at >=
                    std::chrono::seconds(5);
            const bool record_was_removed =
                last_received != std::chrono::steady_clock::time_point{} &&
                received_snapshot_time - last_received >= std::chrono::seconds(1);
            if (never_received_after_start || record_was_removed) {
                m_human_racers_finished[racer_num] = true;
            }
        }

        if (!m_all_human_racers_finished && human_racer_count != 0) {
            bool all_humans_finished = true;
            for (u8 racer_num = 0; racer_num < human_racer_count; racer_num++) {
                if (!m_human_racers_finished[racer_num]) {
                    all_humans_finished = false;
                    break;
                }
            }
            if (all_humans_finished) {
                m_all_human_racers_finished = true;
                m_all_human_racers_finished_at = received_snapshot_time;
                INFO_LOG_FMT(FALCONNECT,
                             "All {} human racers finished; keeping the menu closed for 10 seconds",
                             human_racer_count);
            }
        }

        if (m_all_human_racers_finished && !m_menu_opened_after_finish &&
            received_snapshot_time - m_all_human_racers_finished_at >=
                std::chrono::seconds(10)) {
            if (m_menu_should_reopen_after_finish)
                patcher->OpenMenu();  // OpenMenu writes only the 0x6 Course Select state.
            m_menu_opened_after_finish = true;
            INFO_LOG_FMT(FALCONNECT,
                         "All human racers finished grace period elapsed; menu_reopened={}",
                         m_menu_should_reopen_after_finish);
        }

        std::string race_status = "Falconnect | Ping: " + ping_display + " | PR: " +
                                  std::to_string(packetRate) + "p/s";
        patcher->SetRenderedText(race_status);

        // Check if we've exited the race to the menu
        if (!patcher->memoryReader->ReadIsInRace()) {
            readCounter++;

            if (readCounter > 3) {
                INFO_LOG_FMT(FALCONNECT, "LOAD menu exit detected; stopping race updates player={}",
                             FalconnectSocketManager::instance->playerNumber);
                FalconnectSocketManager::instance->exited = true;

                // Check that we're still in practice mode; if not, disconnect
                u16 mode = patcher->memoryReader->ReadGameMode();
                for (int i = 0; i < 15; i++) mode = patcher->memoryReader->ReadGameMode();

                if (mode != 3) {
                    SuccessAlertFmt("You disconnected from the race.");
                    FalconnectSocketManager::instance->RequestDisconnect();
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
