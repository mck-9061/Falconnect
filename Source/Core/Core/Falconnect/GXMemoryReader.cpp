#include "GXMemoryReader.h"

#include <iostream>

#include "Core/System.h"
#include "Core/PowerPC/PowerPC.h"

GXMemoryReader::GXMemoryReader() : manager(Core::System::GetInstance().GetMemory()) {
}

u32 GXMemoryReader::ReadReferencePointer() {
    constexpr u32 address = 0x800030c8;
    referencePointer = manager.Read_U32(address);

    std::stringstream stream;
    stream << std::hex << referencePointer;

    return referencePointer;
}

u16 GXMemoryReader::ReadGameMode() {
    if (const u16 mode = manager.Read_U16(referencePointer + 0x2453e0); mode != lastReadMode) {
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
    const u16 state = manager.Read_U16(referencePointer + 0x2454e2);
    return state == 7;
}

bool GXMemoryReader::ReadIsInRace() const {
    const u16 state = manager.Read_U16(referencePointer + 0x2454e2);
    return state != 0;
}

RacerMemoryBlock* GXMemoryReader::ReadRacerData(const u8 racerNum) const {
    const u32 baseAddress = manager.Read_U32(referencePointer + 0x227878);

    std::stringstream stream;
    stream << std::hex << baseAddress;

    const u32 address = baseAddress + (racerNum * 0x620);
    //INFO_LOG_FMT(FALCONNECT, "Address: {}", address);

    std::vector<u32> dolphinMemory;

    for (int offset = 0; offset <= 0x620; offset += 0x4) {
        u32 read = manager.Read_U32(address + offset);
        dolphinMemory.push_back(read);
    }

    RacerMemoryBlock* block = RacerMemoryBlock::CreateFromDolphinData(dolphinMemory);
    //INFO_LOG_FMT(FALCONNECT, "State: {}", block->state);

    return block;
}

std::vector<u32> GXMemoryReader::ReadRawRacerData(const u8 racerNum) const
{
  const u32 baseAddress = manager.Read_U32(referencePointer + 0x227878);

  std::stringstream stream;
  stream << std::hex << baseAddress;

  const u32 address = baseAddress + (racerNum * 0x620);

  std::vector<u32> dolphinMemory;

  for (int offset = 0; offset <= 0x620; offset += 0x4)
  {
    u32 read = manager.Read_U32(address + offset);
    dolphinMemory.push_back(read);
  }

  return dolphinMemory;
}

char GXMemoryReader::ReadSelectedRacerID() const {
    const char id = static_cast<char>(manager.Read_U8(referencePointer + 0x2453ef));
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
    const char id = static_cast<char>(manager.Read_U8(referencePointer + 0x245471));
    return id;
}

std::vector<u8> GXMemoryReader::ReadName() const {
    std::vector<u8> name;

    name.reserve(32);
    for (int i = 0; i < 32; i++) {
        name.push_back(manager.Read_U8(referencePointer + 0x230e41 + i));
    }

    return name;
}
