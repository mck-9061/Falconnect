#ifndef DOLPHIN_EMU_GXMEMORYREADER_H
#define DOLPHIN_EMU_GXMEMORYREADER_H
#include <array>
#include <chrono>
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
    std::vector<u8> ReadSelectedCustomMachine() const;

    RacerMemoryBlock *ReadRacerData(u8 racerNum) const;
    std::vector<u32> ReadRawRacerData(u8 racerNum) const;
    // Returns zero when the secondary racer chain is not available.
    u32 ReadSecondaryRacerAddress(u8 racerNum) const;
    static constexpr u32 TIMING_DATA_OFFSET = 0x738;

    u32 referencePointer{};

private:
    Memory::MemoryManager& manager;
    mutable std::array<std::chrono::steady_clock::time_point, 30> m_last_timing_log{};

    u32 lastTime = 0;
    u32 gridTimer = 0;

    u16 modeChangeCount = 0;
    u16 lastReadMode = 0;
};



#endif //DOLPHIN_EMU_GXMEMORYREADER_H
