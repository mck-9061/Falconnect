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
    const u32 secondaryAddress = ReadSecondaryRacerAddress(racerNum);

    if (secondaryAddress != 0) {
        for (u32 i = 0; i < block->timingData.size(); i++)
            block->timingData[i] =
                manager.Read_U32(secondaryAddress + TIMING_DATA_OFFSET + i * sizeof(u32));
        block->raceTimeFrameCount =
            manager.Read_U32(secondaryAddress + RACE_TIME_FRAME_COUNT_OFFSET);
    }

    const auto now = std::chrono::steady_clock::now();

    if (racerNum < m_last_timing_log.size() &&
        now - m_last_timing_log[racerNum] >= std::chrono::seconds(1)) {
        m_last_timing_log[racerNum] = now;
        INFO_LOG_FMT(FALCONNECT,
                     "TIMING READ local_racer={} primary={:08x} secondary={:08x} "
                     "chain={} state={:08x} lap_data=[{:08x},{:08x},{:08x}] total_time_frames={}",
                     racerNum, address, secondaryAddress, secondaryAddress != 0 ? "ok" : "invalid",
                     block->state, block->timingData[0], block->timingData[1], block->timingData[2],
                     block->raceTimeFrameCount);
    }

    //INFO_LOG_FMT(FALCONNECT, "State: {}", block->state);

    return block;
}

u32 GXMemoryReader::ReadSecondaryRacerAddress(const u8 racerNum) const {
    if (racerNum >= 30) return 0;

    const u32 ramEnd = 0x80000000 + manager.GetRamSizeReal();
    const u32 baseAddress = manager.Read_U32(referencePointer + 0x227878);
    if (baseAddress < 0x80000020 || baseAddress > ramEnd) return 0;

    // The local human has a separate block; its +0x540 pointer heads the CPU array.
    const u32 humanAddress = manager.Read_U32(baseAddress - 0x20);
    if (humanAddress < 0x80000000 ||
        static_cast<u64>(humanAddress) + RACE_TIME_FRAME_COUNT_OFFSET + sizeof(u32) > ramEnd) return 0;
    if (racerNum == 0) return humanAddress;

    const u32 cpuBaseAddress = manager.Read_U32(humanAddress + 0x540);
    if (cpuBaseAddress < 0x80000000) return 0;

    // Local racer 1 is the first CPU block. Use wide arithmetic before validating the range.
    const u64 cpuAddress = static_cast<u64>(cpuBaseAddress) + (racerNum - 1) * 0x820;
    if (cpuAddress + RACE_TIME_FRAME_COUNT_OFFSET + sizeof(u32) > ramEnd) return 0;
    return static_cast<u32>(cpuAddress);
}

std::vector<u32> GXMemoryReader::ReadRawRacerData(const u8 racerNum) const
{
  const u32 baseAddress = manager.Read_U32(referencePointer + 0x227878);

    if (baseAddress < 0x80000000 || baseAddress > 0x81000000) {
        // INFO_LOG_FMT(FALCONNECT, "Invalid racer pointer!");
        std::vector<u32> dolphinMemory;
        dolphinMemory.push_back(0);
        return dolphinMemory;
    }

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

    const std::vector<u32> data = ReadRawRacerData(0);

    if (data[0] == 0) {
        lastTime = time(nullptr);
        return false;
    }

    if (time(nullptr) - lastTime > 1) {
        lastTime = time(nullptr);
    }

    if (data[0] & 0x0000FF00) {
        gridTimer += (time(nullptr) - lastTime);
        // INFO_LOG_FMT(FALCONNECT, "{}", gridTimer);
    } else {
        gridTimer = 0;
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

std::vector<u8> GXMemoryReader::ReadSelectedCustomMachine() const {
    std::vector<u8> data;

    data.push_back(manager.Read_U8(referencePointer + 0x1c7664)); // body
    data.push_back(manager.Read_U8(referencePointer + 0x1c766c)); // cockpit
    data.push_back(manager.Read_U8(referencePointer + 0x1c7674)); // booster

    return data;
}

std::vector<u8> GXMemoryReader::ReadName() const {
    std::vector<u8> name;

    name.reserve(32);
    for (int i = 0; i < 32; i++) {
        name.push_back(manager.Read_U8(referencePointer + 0x230e42 + i));
    }

    return name;
}
