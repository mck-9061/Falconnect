#include "GXMemoryReader.h"

#include <iostream>

#include "Core/System.h"
#include "Core/PowerPC/PowerPC.h"

GXMemoryReader::GXMemoryReader(const Core::CPUThreadGuard &cpuGuard) : guard(cpuGuard), interface(Core::System::GetInstance().GetPowerPC().GetDebugInterface()) {
}

u32 GXMemoryReader::ReadReferencePointer() {
    constexpr u32 address = 0x800030c8;
    referencePointer = interface.ReadMemory(guard, address);

    std::stringstream stream;
    stream << std::hex << referencePointer;
    INFO_LOG_FMT(FALCONNECT, "Read: {}", stream.str());

    return referencePointer;
}

u8 GXMemoryReader::Read8(const u32 offset) const {
    const u32 address = referencePointer + offset;
    const u32 mem = interface.ReadMemory(guard, address);

    const u8 read = static_cast<u8>(mem >> 24);

    return read;
}

u16 GXMemoryReader::Read16(const u32 offset) const {
    const u32 address = referencePointer + offset;
    const u32 mem = interface.ReadMemory(guard, address);

    const u16 read = static_cast<u8>(mem >> 16);

    return read;
}

u16 GXMemoryReader::ReadGameMode() const {
    const u16 mode = Read16(0x2453e0);
    return mode;
}

bool GXMemoryReader::ReadSettingsSelectedFlag() const {
    const u16 state = Read16(0x2454e2);
    return state == 7;
}

RacerMemoryBlock* GXMemoryReader::ReadRacerData(const u8 racerNum) const {
    const u32 baseAddress = interface.ReadMemory(guard, referencePointer + 0x227878);

    std::stringstream stream;
    stream << std::hex << baseAddress;
    INFO_LOG_FMT(FALCONNECT, "Base address: {}", stream.str());

    const u32 address = baseAddress + (racerNum * 0x620);

    std::vector<u32> dolphinMemory;

    for (int offset = 0; offset <= 0x620; offset += 0x4) {
        u32 read = interface.ReadMemory(guard, address + offset);
        dolphinMemory.push_back(read);
    }

    RacerMemoryBlock* block = RacerMemoryBlock::CreateFromDolphinData(dolphinMemory);

    std::stringstream stream2;
    stream2 << std::hex << block->energy;
    INFO_LOG_FMT(FALCONNECT, "Current energy: {}", stream2.str());

    return block;
}

u8 GXMemoryReader::ReadSelectedRacerID() const {
    const u8 id = Read8(0x2453ef);
    return id;
}
