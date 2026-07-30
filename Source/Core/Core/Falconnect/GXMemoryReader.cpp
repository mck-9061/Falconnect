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

u16 GXMemoryReader::ReadGameMode() {
    const u16 mode = Read16(0x2453e0);
    if (mode != lastReadMode) {
        modeChangeCount++;

        if (modeChangeCount > 10) {
            lastReadMode = mode;
            modeChangeCount = 0;
        }
    } else {
        modeChangeCount = 0;
    }

    return lastReadMode;
}

bool GXMemoryReader::ReadSettingsSelectedFlag() const {
    const u16 state = Read16(0x2454e2);
    return state == 7;
}

bool GXMemoryReader::ReadIsInRace() const {
    const u16 state = Read16(0x2454e2);
    return state != 0;
}

RacerMemoryBlock* GXMemoryReader::ReadRacerData(const u8 racerNum) const {
    const u32 baseAddress = interface.ReadMemory(guard, referencePointer + 0x227878);

    std::stringstream stream;
    stream << std::hex << baseAddress;

    const u32 address = baseAddress + (racerNum * 0x620);

    std::vector<u32> dolphinMemory;

    for (int offset = 0; offset <= 0x620; offset += 0x4) {
        u32 read = interface.ReadMemory(guard, address + offset);
        dolphinMemory.push_back(read);
    }

    RacerMemoryBlock* block = RacerMemoryBlock::CreateFromDolphinData(dolphinMemory);

    return block;
}

std::vector<u32> GXMemoryReader::ReadRawRacerData(const u8 racerNum) const
{
  const u32 baseAddress = interface.ReadMemory(guard, referencePointer + 0x227878);

  std::stringstream stream;
  stream << std::hex << baseAddress;

  const u32 address = baseAddress + (racerNum * 0x620);

  std::vector<u32> dolphinMemory;

  for (int offset = 0; offset <= 0x620; offset += 0x4)
  {
    u32 read = interface.ReadMemory(guard, address + offset);
    dolphinMemory.push_back(read);
  }

  return dolphinMemory;
}

char GXMemoryReader::ReadSelectedRacerID() const {
    const char id = static_cast<char>(Read8(0x2453ef));
    return id;
}

bool GXMemoryReader::HasGridded() {
    if (lastTime == 0) lastTime = time(nullptr);

    if (const std::vector<u32> data = ReadRawRacerData(0); data[0] & 0x0000FF00) {
        gridTimer += (time(nullptr) - lastTime);
        INFO_LOG_FMT(FALCONNECT, "{}", gridTimer);
    }

    if (gridTimer > 3) {
        lastTime = 0;
        gridTimer = 0;
        return true;
    }

    lastTime = time(nullptr);
    return false;
}

u8 GXMemoryReader::ReadSelectedCourse() const {
    const char id = static_cast<char>(Read8(0x245471));
    return id;
}

std::vector<u8> GXMemoryReader::ReadName() const {
    std::vector<u8> name;

    name.reserve(32);
    for (int i = 0; i < 32; i++) {
        name.push_back(Read8(0x230e41 + i));
    }

    return name;
}
