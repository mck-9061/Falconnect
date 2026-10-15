#ifndef DOLPHIN_EMU_GXMEMORYPATCHER_H
#define DOLPHIN_EMU_GXMEMORYPATCHER_H
#include <array>
#include <chrono>

#include "GXMemoryReader.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/Memmap.h"


class GXMemoryReader;

class GXMemoryPatcher {
public:
    explicit GXMemoryPatcher();

    void Initialise();

    // Patch methods
    void DisableB() const;
    void DisableOptionsMenuControl() const;
    void FullyDisableMenuControl() const;
    void ReEnableMenuControl() const;
    void SetPracticeModeText(std::string text) const;
    void StartRaceFromPracticeOptions() const;
    void ForceStartRace() const;
    void DisableAIControl() const;
    void DisableCountdown() const;
    void StartCountdown() const;
    void InitialiseText() const;
    void SetBoostLap(u8 lap) const;
    void SetOpponentRacerIds(const u8 racerIDs[]) const;
    void SetRenderedText(const std::string &text) const;
    void SetDefaultRaceSettings() const;
    // received_center_position is written only for a timing-accepted snapshot. Rejected
    // snapshots still update state and inputs, but leave position and rotation untouched.
    void SetRacerData(u8 racerNum, const RacerMemoryBlock& patchData, bool full,
                      const std::array<u32, 3>& received_center_position);
    void SetRacerMachineName(u8 racerNum, const std::vector<u8> &name) const;
    void SetGrid() const;
    void SetCourse(u8 courseID) const;
    void SetCpuCount(u8 cpuCount) const;
    void SetLapCount(u8 lapCount) const;
    void ConstrainMenu() const;
    void InitialiseNameLabels() const;
    void ResetToTitle() const;
    void EnablePositionAnnouncementsInPractice() const;
    void EnableAIControlFor(u8 start, u8 count);
    void StopPhysicsOnReceivedMachines() const;
    void SetPlayersReadyText(u8 ready, u8 total) const;
    void SetupCustomMachines() const;
    void SetCustomMachineData(const std::vector<u8>& machine_data) const;
    void SetSelectedRacerId(u8 racer_id) const;
    void SetSelectedMachine(u8 racer_id, const std::vector<u8>& custom_parts) const;

    u32 referencePointer{};
    bool isReady{};
    GXMemoryReader* memoryReader;

private:
    Memory::MemoryManager& manager;
    std::array<std::chrono::steady_clock::time_point, 30> m_last_timing_log{};
    std::array<std::array<u32, 3>, 30> m_last_written_timing{};
    std::array<bool, 30> m_has_written_timing{};

    u32 racerBaseAddress;

    void WriteU8Vector(const std::vector<u8> &in, u32 address) const;
};



#endif //DOLPHIN_EMU_GXMEMORYPATCHER_H
