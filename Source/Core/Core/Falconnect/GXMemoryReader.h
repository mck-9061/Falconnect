#ifndef DOLPHIN_EMU_GXMEMORYREADER_H
#define DOLPHIN_EMU_GXMEMORYREADER_H
#include "RacerMemoryBlock.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"


class GXMemoryReader {
public:
    explicit GXMemoryReader(const Core::CPUThreadGuard& cpuGuard);

    u32 ReadReferencePointer();
    u8 Read8(u32 offset) const;
    [[nodiscard]] u16 Read16(u32 offset) const;

    [[nodiscard]] u16 ReadGameMode() const;
    [[nodiscard]] bool ReadSettingsSelectedFlag() const;
    u8 ReadSelectedRacerID() const;

    RacerMemoryBlock *ReadRacerData(u8 racerNum) const;

    u32 referencePointer{};

private:
    const Core::CPUThreadGuard& guard;
    PPCDebugInterface& interface;
};



#endif //DOLPHIN_EMU_GXMEMORYREADER_H
