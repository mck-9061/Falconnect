#ifndef DOLPHIN_EMU_GXMEMORYREADER_H
#define DOLPHIN_EMU_GXMEMORYREADER_H
#include "RacerMemoryBlock.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/Memmap.h"


class GXMemoryReader {
public:
    explicit GXMemoryReader();

    u32 ReadReferencePointer();

    [[nodiscard]] u16 ReadGameMode();
    [[nodiscard]] bool ReadSettingsSelectedFlag() const;
    [[nodiscard]] bool ReadIsInRace() const;
    [[nodiscard]] char ReadSelectedRacerID() const;
    [[nodiscard]] bool HasGridded();
    u8 ReadSelectedCourse() const;
    std::vector<u8> ReadName() const;

    RacerMemoryBlock *ReadRacerData(u8 racerNum) const;
    std::vector<u32> ReadRawRacerData(u8 racerNum) const;

    u32 referencePointer{};

private:
    Memory::MemoryManager& manager;

    u32 lastTime = 0;
    u32 gridTimer = 0;

    u16 modeChangeCount = 0;
    u16 lastReadMode = 0;
};



#endif //DOLPHIN_EMU_GXMEMORYREADER_H
