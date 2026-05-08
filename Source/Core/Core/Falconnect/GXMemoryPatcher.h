#ifndef DOLPHIN_EMU_GXMEMORYPATCHER_H
#define DOLPHIN_EMU_GXMEMORYPATCHER_H
#include "GXMemoryReader.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"


class GXMemoryReader;

class GXMemoryPatcher {
public:
    explicit GXMemoryPatcher(const Core::CPUThreadGuard& cpuGuard);

    void Initialise();

    // Patch methods
    void DisableMenuControl() const;
    void StartRaceFromPracticeOptions() const;
    void DisableAIControl() const;
    void DisableCountdown() const;
    void StartCountdown() const;
    void InitialiseText() const;
    void SetBoostLap(u8 lap) const;
    void SetOpponentRacerId(u8 racerID) const;
    void SetRenderedText(const std::string &text) const;

    u32 referencePointer{};
    bool isReady{};
    GXMemoryReader* memoryReader;

private:
    const Core::CPUThreadGuard& guard;
    PPCDebugInterface& interface;
};



#endif //DOLPHIN_EMU_GXMEMORYPATCHER_H
