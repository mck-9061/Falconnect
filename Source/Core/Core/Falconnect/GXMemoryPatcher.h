#ifndef DOLPHIN_EMU_GXMEMORYPATCHER_H
#define DOLPHIN_EMU_GXMEMORYPATCHER_H
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"


class GXMemoryPatcher {
public:
    explicit GXMemoryPatcher(const Core::CPUThreadGuard& cpuGuard);

    void Initialise();
    void LoadReferencePointer();
    [[nodiscard]] u16 Read16(u32 offset) const;

    // Patch methods
    void DisableMenuControl() const;
    void DisableAIControl();
    void DisableCountdown();
    void InitialiseText() const;
    void SetBoostLap() const;

    u32 referencePointer{};
    bool isReady{};

private:
    const Core::CPUThreadGuard& guard;
    PPCDebugInterface& interface;
};



#endif //DOLPHIN_EMU_GXMEMORYPATCHER_H
