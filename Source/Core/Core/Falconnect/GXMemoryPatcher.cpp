#include "GXMemoryPatcher.h"

#include "FalconnectSocketManager.h"
#include "GXMemoryReader.h"
#include "PowerPCScripts.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/PowerPC.h"

GXMemoryPatcher::GXMemoryPatcher() :
                                                                         manager(
                                                                             Core::System::GetInstance().GetMemory()) {
    memoryReader = new GXMemoryReader();

    Initialise();
}

std::vector<uint32_t> stringToUint32Array(const std::string& str)
{
    std::vector<uint32_t> result;

    for (size_t i = 0; i < str.size(); i += 4)
    {
        uint32_t value = 0;

        for (size_t j = 0; j < 4; ++j)
        {
            value <<= 8;

            if (i + j < str.size())
            {
                value |= static_cast<unsigned char>(str[i + j]);
            }
        }

        result.push_back(value);
    }

    return result;
}

void GXMemoryPatcher::Initialise() {
    INFO_LOG_FMT(FALCONNECT, "Loading reference pointer...");

    referencePointer = memoryReader->ReadReferencePointer();

    if (referencePointer == 0 || referencePointer & 0x01000000) {
        INFO_LOG_FMT(FALCONNECT, "Reference pointer invalid or not yet defined!");
        isReady = false;
        return;
    }

    INFO_LOG_FMT(FALCONNECT, "Reference pointer located.");
    isReady = true;
}

void GXMemoryPatcher::WriteU8Vector(const std::vector<u8> &in, const u32 address) const {
    u16 i = 0;

    for (const u8 d : in) {
        manager.Write_U8(d, address + i);
        i++;
    }

    Core::System::GetInstance().GetJitInterface().InvalidateICache(address, i, true);
}

void GXMemoryPatcher::DisableOptionsMenuControl() const {
    const u32 menuControlAddress = referencePointer + 0x3e28f0;
    manager.Write_U32(0x3c608060, menuControlAddress);
    manager.Write_U32(0x60000000, menuControlAddress + 4);
    manager.Write_U32(0x88630000, menuControlAddress + 8);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(menuControlAddress, 12, true);
}

void GXMemoryPatcher::FullyDisableMenuControl() const {
    const u32 address = referencePointer + 0x3e2604;
    manager.Write_U32(0x60000000, address);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(address, 4, true);
}

void GXMemoryPatcher::ReEnableMenuControl() const {
    const u32 address = referencePointer + 0x3e2604;
    manager.Write_U32(0x480056d1, address);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(address, 4, true);
}

void GXMemoryPatcher::SetPracticeModeText(std::string text) const {
    const u32 pointerAddress = referencePointer + 0x406f8c;
    const u32 textPosition = 0x805cf700;
    std::vector<u32> ascii = stringToUint32Array(text);

    // Blank any existing text
    for (int offset = 0; offset < 100; offset++) {
        const u32 address = textPosition + (offset * 4);
        manager.Write_U32(0x0, address);
    }

    for (u8 offset = 0; offset < ascii.size(); offset++) {
        const u32 address = textPosition + (offset * 4);
        manager.Write_U32(ascii[offset], address);
    }

    // Replace pointer
    manager.Write_U32(textPosition, pointerAddress);
}

void GXMemoryPatcher::DisableAIControl() const {
    // Set code to set AI movement per CPU
    const u32 freeAddress = referencePointer + 0x1cd1a0;
    manager.Write_U32(0x7de802a6, freeAddress);
    manager.Write_U32(0x8a0300e0, freeAddress + 4);
    manager.Write_U32(0x2c100000, freeAddress + 8);
    manager.Write_U32(0x41820008, freeAddress + 12);
    manager.Write_U32(0x4bef9e01, freeAddress + 16);
    manager.Write_U32(0x7de803a6, freeAddress + 20);
    manager.Write_U32(0x4e800020, freeAddress + 24);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(freeAddress, 28, true);

    const u32 aiMoveAddress = referencePointer + 0x839a0;

    manager.Write_U32(0x48149801, aiMoveAddress);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(aiMoveAddress, 4, true);
}

void GXMemoryPatcher::EnableAIControlFor(const u8 start, const u8 count) {
    if (start == 0) return;

    u32 baseAddress = manager.Read_U32(referencePointer + 0x227878);
    if (baseAddress < 0x80000000) {
        INFO_LOG_FMT(FALCONNECT, "Invalid address!");
        baseAddress = racerBaseAddress;
        if (baseAddress < 0x80000000) {
            INFO_LOG_FMT(FALCONNECT, "Invalid address!");
            return;
        }
    }

    racerBaseAddress = baseAddress;

    for (int i = 0; i < count; i++) {
        const u32 address = baseAddress + ((start + i) * 0x620);
        manager.Write_U8(0x1, address + 0xe0);
    }
}

void GXMemoryPatcher::DisableCountdown() const {
    const u32 countdownAddress = referencePointer + 0x34a30;

    manager.Write_U32(0x48000150, countdownAddress);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(countdownAddress, 4, true);
}

void GXMemoryPatcher::InitialiseText() const {
    // Add text function to unused section of memory
    constexpr u32 functionBaseAddress = 0x80400000;

    WriteU8Vector(PowerPCScripts::CustomStringScript, functionBaseAddress);

    //Core::System::GetInstance().GetJitInterface().InvalidateICache(functionBaseAddress, 200, true);

    // Create entry point for text function
    const u32 entryPointAddress = referencePointer + 0xca44c;

    const u32 offsetFromBase = functionBaseAddress - entryPointAddress;
    const u32 instruction = 0x48000000 + offsetFromBase + 1;

    manager.Write_U32(instruction, entryPointAddress);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(entryPointAddress, 4, true);

    // // Add text
    // for (int offset = 0; offset < 11; offset++) {
    //     constexpr u32 textAddress = 0x80390000;
    //     const u32 address = textAddress + (offset * 4);
    //     manager.Write_U32(address, PowerPCScripts::InitialText[offset]);
    // }
    SetRenderedText("Falconnect C++ Port Test | Do a barrel roll!");

    // Set text position
    constexpr u32 positionFloatsAddress = 0x80430000;

    constexpr float x = 320;
    constexpr float y = 5;
    constexpr auto bitsX = std::bit_cast<uint32_t>(x);
    constexpr auto bitsY = std::bit_cast<uint32_t>(y);

    manager.Write_U32(bitsX, positionFloatsAddress);
    manager.Write_U32(bitsY, positionFloatsAddress + 8);
}

void GXMemoryPatcher::SetBoostLap(const u8 lap) const {
    manager.Write_U32(0x60000000, referencePointer + 0x3294c); // NOP check for practice mode on lap 2 boost announcement
    manager.Write_U32(0x60000000, referencePointer + 0x34b68); // NOP announcement at start of practice mode
    manager.Write_U32(0x60000000, referencePointer + 0x33300); // NOP practice mode check in boost

    manager.Write_U32(0x4800017c, referencePointer + 0xc91ec); // Nullify check for practice mode in energy bar render

    manager.Write_U32(0x2c000000 + lap, referencePointer + 0x3330C); // Set lap check for boost
    manager.Write_U32(0x2c000001 + lap, referencePointer + 0xc91d8); // Set lap check for energy bar render
    manager.Write_U32(0x281e0000 + lap, referencePointer + 0x32938); // Set lap check for announcement

    // Invalidations
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x3294c, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x34b68, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x33300, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0xc91ec, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x3330C, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0xc91d8, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x32938, 4, true);
}

void GXMemoryPatcher::StartRaceFromPracticeOptions() const {
    manager.Write_U32(0x01010101, 0x80600000);
}

void GXMemoryPatcher::StartCountdown() const {
    const u32 countdownAddress = referencePointer + 0x34a30;

    manager.Write_U32(0x40820150, countdownAddress);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(countdownAddress, 4, true);
}

void GXMemoryPatcher::SetOpponentRacerIds(const u8 racerIDs[]) const {
    const u32 idLoadAddress = referencePointer + 0x00034E40;

    // Replace instruction to load the correct racer ID to always load given ID
    //manager.Write_U32(idLoadAddress, 0x3AA00000 + racerID);

    // Load array into memory
    for (int i = 0; i < 31; i++) {
        const u32 address = 0x803d1a00 + i;
        if (i == 0) {
            manager.Write_U8(0, address); // index
            continue;
        }

        u8 data = racerIDs[i - 1];
        if (data == 0) data = 0x06;

        manager.Write_U8(data, address);
    }

    // Create function to read IDs from array
    constexpr u32 functionAddress = 0x803d1aa0;

    manager.Write_U32(0x3dc0803d, functionAddress);
    manager.Write_U32(0x61cf1a00, functionAddress + 4);
    manager.Write_U32(0x8a0f0000, functionAddress + 8);
    manager.Write_U32(0x3a300001, functionAddress + 12);
    manager.Write_U32(0x9a2f0000, functionAddress + 16);
    manager.Write_U32(0x7eaf88ae, functionAddress + 20);
    manager.Write_U32(0x39c00000, functionAddress + 24);
    manager.Write_U32(0x61cf0000, functionAddress + 28);
    manager.Write_U32(0x7dee7b78, functionAddress + 32);
    manager.Write_U32(0x7df07b78, functionAddress + 36);
    manager.Write_U32(0x7df17b78, functionAddress + 40);
    manager.Write_U32(0x4e800020, functionAddress + 44);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(functionAddress, 48, true);

    // Set jump instruction
    manager.Write_U32(0x48000001 + (functionAddress - idLoadAddress), idLoadAddress);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(idLoadAddress, 4, true);

    const u32 racer_check_address = idLoadAddress + 60;

    // Remove duplicate racer check
    manager.Write_U32(0x60000000, racer_check_address);
    manager.Write_U32(0x60000000, racer_check_address + 8);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(racer_check_address, 12, true);
}

void GXMemoryPatcher::SetRenderedText(const std::string &text) const {
    // Blank previous text
    //INFO_LOG_FMT(FALCONNECT, "Blanking text");
    for (int offset = 0; offset < 100; offset++) {
        constexpr u32 textAddress = 0x80390000;
        const u32 address = textAddress + (offset * 4);
        manager.Write_U32(0x0, address);
    }

    //INFO_LOG_FMT(FALCONNECT, "Setting text");
    const std::vector<u32> rep = stringToUint32Array(text);

    for (u8 offset = 0; offset < rep.size(); offset++) {
        constexpr u32 textAddress = 0x80390000;
        const u32 address = textAddress + (offset * 4);
        manager.Write_U32(rep[offset], address);
    }
}

void GXMemoryPatcher::SetDefaultRaceSettings() const {
    manager.Write_U8(0x04, referencePointer + 0x24550d); // 4 CPU
    manager.Write_U8(0x01, referencePointer + 0x245517); // Allow restore
    manager.Write_U8(0x03, referencePointer + 0x24551b); // 3 laps
    manager.Write_U8(0x03, referencePointer + 0x2453e9); // Master
    manager.Write_U8(0x03, referencePointer + 0x2453eb); // Master
}

void GXMemoryPatcher::SetCpuCount(const u8 cpuCount) const {
    manager.Write_U8(cpuCount, referencePointer + 0x24550d);
}

void GXMemoryPatcher::SetRacerData(const u8 racerNum, const RacerMemoryBlock &patchData, bool full) {
    if (racerNum == 0) return;

  u32 baseAddress = manager.Read_U32(referencePointer + 0x227878) + (racerNum * 0x620);

    INFO_LOG_FMT(FALCONNECT, "Base racer address: 0x{}", std::format("{:x}", baseAddress));

    if (baseAddress < 0x80000000) {
        INFO_LOG_FMT(FALCONNECT, "Invalid address!");
        baseAddress = racerBaseAddress;
        if (baseAddress < 0x80000000) {
            INFO_LOG_FMT(FALCONNECT, "Invalid address!");
            return;
        }
    }

    racerBaseAddress = baseAddress;



  manager.Write_U32(patchData.state, baseAddress);
    // Make sure the game thinks it's an AI so it isn't trying to update the inputs
    manager.Write_U8(0x04, baseAddress);

    if (full) {
        manager.Write_U32(patchData.centerPosition[0], baseAddress + (31 * 4));
        manager.Write_U32(patchData.centerPosition[1], baseAddress + (32 * 4));
        manager.Write_U32(patchData.centerPosition[2], baseAddress + (33 * 4));
        //
        manager.Write_U32(patchData.lastCenterPosition[0], baseAddress + (34 * 4));
        manager.Write_U32(patchData.lastCenterPosition[1], baseAddress + (35 * 4));
        manager.Write_U32(patchData.lastCenterPosition[2], baseAddress + (36 * 4));
        //
        // manager.Write_U32(baseAddress + 0x1e0, patchData.lastCenterPosition[0]);
        // manager.Write_U32(baseAddress + 0x1e4, patchData.lastCenterPosition[1]);
        // manager.Write_U32(baseAddress + 0x1e8, patchData.lastCenterPosition[2]);

        manager.Write_U32(patchData.centerPosOscX, baseAddress + 0x158);
        manager.Write_U32(patchData.centerPosOscY, baseAddress + 0x168);
        manager.Write_U32(patchData.centerPosOscZ, baseAddress + 0x178);

        // set last center position to received center position?
        // manager.Write_U32(baseAddress + (34 * 4), patchData.centerPosition[0]);
        // manager.Write_U32(baseAddress + (35 * 4), patchData.centerPosition[1]);
        // manager.Write_U32(baseAddress + (36 * 4), patchData.centerPosition[2]);

        manager.Write_U32(patchData.velocityWorld[0], baseAddress + (37 * 4));
        manager.Write_U32(patchData.velocityWorld[1], baseAddress + (38 * 4));
        manager.Write_U32(patchData.velocityWorld[2], baseAddress + (39 * 4));
        //
        manager.Write_U32(patchData.velocityMachine[0], baseAddress + (46 * 4));
        manager.Write_U32(patchData.velocityMachine[1], baseAddress + (47 * 4));
        manager.Write_U32(patchData.velocityMachine[2], baseAddress + (48 * 4));
        //manager.Write_U32(baseAddress + 0xd4, patchData.velocityMachine[2]);

        manager.Write_U32(patchData.orientationWorld[0], baseAddress + (59 * 4));
        manager.Write_U32(patchData.orientationWorld[1], baseAddress + (60 * 4));
        manager.Write_U32(patchData.orientationWorld[2], baseAddress + (61 * 4));

        manager.Write_U32(patchData.upVector[0], baseAddress + (63 * 4));
        manager.Write_U32(patchData.upVector[1], baseAddress + (64 * 4));
        manager.Write_U32(patchData.upVector[2], baseAddress + (65 * 4));

        manager.Write_U32(patchData.orientationGravity[0], baseAddress + (67 * 4));
        manager.Write_U32(patchData.orientationGravity[1], baseAddress + (68 * 4));
        manager.Write_U32(patchData.orientationGravity[2], baseAddress + (69 * 4));

        manager.Write_U32(patchData.speed, baseAddress + (95 * 4));
        manager.Write_U32(patchData.arialTilt, baseAddress + (96 * 4));
        //
        manager.Write_U32(patchData.trackOrientation[0], baseAddress + (111 * 4));
        manager.Write_U32(patchData.trackOrientation[1], baseAddress + (112 * 4));
        manager.Write_U32(patchData.trackOrientation[2], baseAddress + (113 * 4));

        manager.Write_U32(patchData.bottomPosition[0], baseAddress + (117 * 4));
        manager.Write_U32(patchData.bottomPosition[1], baseAddress + (118 * 4));
        manager.Write_U32(patchData.bottomPosition[2], baseAddress + (119 * 4));
    }

    manager.Write_U32(patchData.energy, baseAddress + (97 * 4));

  manager.Write_U32(patchData.inputs[0], baseAddress + (123 * 4));
  manager.Write_U32(patchData.inputs[1], baseAddress + (124 * 4));
  manager.Write_U32(patchData.inputs[2], baseAddress + (125 * 4));
  INFO_LOG_FMT(FALCONNECT, "Accelerator input: {}", patchData.inputs[5]);
  manager.Write_U32(patchData.inputs[3], baseAddress + (126 * 4));
  manager.Write_U32(patchData.inputs[4], baseAddress + (127 * 4));
  manager.Write_U32(patchData.inputs[5], baseAddress + (128 * 4));
  manager.Write_U32(patchData.inputs[6], baseAddress + (129 * 4));

  manager.Write_U32(patchData.sideAttack, baseAddress + (388 * 4));

    manager.Write_U32(patchData.maxSpeedKmh, baseAddress + (54 * 4));
    manager.Write_U32(patchData.acceleration, baseAddress + (136 * 4));
    manager.Write_U32(patchData.baseSpeed, baseAddress + (137 * 4));
    manager.Write_U32(patchData.maxSpeed, baseAddress + (139 * 4));

    manager.Write_U32(patchData.restoreFlag, baseAddress + (12 * 4));
}

void GXMemoryPatcher::SetRacerMachineName(const u8 racerNum, const std::vector<u8> &name) const {
    const u32 baseAddress = manager.Read_U32(referencePointer + 0x227878) + (racerNum * 0x620);

    INFO_LOG_FMT(FALCONNECT, "Base racer address: 0x{}", std::format("{:x}", baseAddress));

    if (baseAddress < 0x80000000) {
        INFO_LOG_FMT(FALCONNECT, "Invalid address!");
        return;
    }

    WriteU8Vector(name, baseAddress + 0x3c);
}

void GXMemoryPatcher::SetGrid() const {
    // Clear the function used to arrange the grid array, then put in our own order
    // I am SO fucking proud of finding that function it's unreal it runs like a million times a frame for no reason

    // preserving this cus it's funny even if it is wrong and overwrites a common function that yeah come to think of it why would there be a specific array permutation algorithm just for grid arranging when it could be a common function like yeah obviously dumbass UGHHHHHHHH

    // Find the function in probably the hackiest code ever written but it's 1am
    // const u32 callingFunctionAddress = referencePointer + 0x85bf8;
    // const u32 callingInstruction = interface.ReadMemory(guard, callingFunctionAddress);
    //
    // u32 offset = (callingInstruction << 6) >> 6;
    // offset = offset & 0b00000001111111111111111111111111;
    // offset -= 0b00000010000000000000000000000001;
    //
    // u32 address = callingFunctionAddress + offset;
    //
    // const std::string s = std::format("{:x}", address);
    // INFO_LOG_FMT(FALCONNECT, "{}", s);
    //
    // for (int i = 0; i < 30; i++) {
    //     const u16 numToInsert = positions[i];
    //     const u32 loadInstruction = 0x39c00000 + numToInsert;
    //     const u32 storeInstruction = 0xb1c30000 + (i * 2);
    //
    //     manager.Write_U32(address + (i * 8), loadInstruction);
    //     manager.Write_U32(address + (i * 8) + 4, storeInstruction);
    // }
    //
    // manager.Write_U32(address + (30 * 8), 0x39c00000); // Reset r14
    // manager.Write_U32(address + (30 * 8) + 4, 0x4e800020); // Return

    const u32 address = referencePointer + 0x85bf8;
    const u32 offsetToFreeAddress = 0x80376a00 - address;
    const u32 jumpInstruction = 0x48000000 + offsetToFreeAddress + 1;
    const u8 playerNum = FalconnectSocketManager::instance->playerNumber;

    for (int i = 0; i < 30; i++) {
        u8 playerNumToInsert = i;
        if (i == 0) playerNumToInsert = playerNum - 1;
        if (i == playerNum - 1) playerNumToInsert = 0;

        const u32 loadInstruction = 0x39c00000 + playerNumToInsert;
        const u32 storeInstruction = 0xb1c30000 + (i * 2);

        manager.Write_U32(loadInstruction, 0x80376a00 + (i * 8));
        manager.Write_U32(storeInstruction, 0x80376a00 + (i * 8) + 4);

        Core::System::GetInstance().GetJitInterface().InvalidateICache(0x80376a00 + (i * 8), 8, true);
    }

    manager.Write_U32(0x39c00000, 0x80376a00 + (30 * 8)); // Reset r14
    manager.Write_U32(0x4e800020, 0x80376a00 + (30 * 8) + 4); // Return

    Core::System::GetInstance().GetJitInterface().InvalidateICache(0x80376a00 + (30 * 8), 8, true);

    manager.Write_U32(jumpInstruction, address);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(address, 4, true);
}

void GXMemoryPatcher::SetCourse(const u8 courseID) const {
    const u32 address = referencePointer + 0x245471;
    manager.Write_U8(courseID, address);
}

void GXMemoryPatcher::ConstrainMenu() const {
    const u32 address = referencePointer + 0x1bf144;

    if (const u8 current = manager.Read_U8(referencePointer + 0x1bf144); current == 3 || current == 5) {
        manager.Write_U8(6, address);
    }
}

void GXMemoryPatcher::InitialiseNameLabels() const {
    // Set font
    manager.Write_U8(0x03, referencePointer + 0x12a217);

    // Force all drivers to be rivals
    manager.Write_U32(0x3ae00001, referencePointer + 0x129bc4);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x129bc4, 4, true);

    // Branch to new code to load pointer
    manager.Write_U32(0x48092d65, referencePointer + 0x129c3c);
    manager.Write_U32(0x48092d19, referencePointer + 0x129c88);
    manager.Write_U32(0x48092cbd, referencePointer + 0x129ce4);
    manager.Write_U32(0x48092c91, referencePointer + 0x129d10);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x129c3c, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x129c88, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x129ce4, 4, true);
    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x129d10, 4, true);

    // New code: Load pointer into r5
    // pointer = base pointer + (0x20 * r27)
    // 1d 1b 00 20
    // 3c a0 80 37
    // 60 a5 7d 00
    // 7c a5 42 14
    // 39 00 00 00
    // 4e 80 00 20

    const u32 codeAddress = referencePointer + 0x1bc9a0;

    manager.Write_U32(0x1d1b0020, codeAddress);
    manager.Write_U32(0x3ca08037, codeAddress + 4);
    manager.Write_U32(0x60a57d00, codeAddress + 8);
    manager.Write_U32(0x7ca54214, codeAddress + 12);
    manager.Write_U32(0x39000000, codeAddress + 16);
    manager.Write_U32(0x4e800020, codeAddress + 20);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(codeAddress, 24, true);

    // Set names
    for (int i = 0; i < 30; i++) {
        u8 usedIndex = i;
        if (i == 0) usedIndex = FalconnectSocketManager::instance->playerNumber - 1;
        if (i == FalconnectSocketManager::instance->playerNumber - 1) {
            INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);
            // Do want to put our own name in though
            SetRacerMachineName(0, FalconnectSocketManager::instance->names[i]);

            continue; // Skip our data
        }

        const u32 nameAddress = 0x80377d00 + usedIndex * 32;
        WriteU8Vector(FalconnectSocketManager::instance->names[i], nameAddress);
        SetRacerMachineName(usedIndex, FalconnectSocketManager::instance->names[i]);
    }
}

void GXMemoryPatcher::ResetToTitle() const {
    Core::System::GetInstance().GetPowerPC().GetDebugInterface().SetPC(0x80003154);
    //manager.Write_U32(referencePointer + 0x245474, 0x24000100);
}

void GXMemoryPatcher::EnablePositionAnnouncementsInPractice() const {
    // Bypass mode check in finish announcement
    manager.Write_U32(0x3b80012c, referencePointer + 0x15c81c);
    manager.Write_U32(0x3b80012c, referencePointer + 0x15c82c);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x15c81c, 0x14, true);
}

void GXMemoryPatcher::StopPhysicsOnReceivedMachines() const {
    // Add code to only update machine if 0xe0 == 1 or 0x00 == 80
    const u32 codeAddress = referencePointer + 0x1d63a0;

    manager.Write_U32(0x7de802a6, codeAddress);
    manager.Write_U32(0x8a0300e0, codeAddress + 4);
    manager.Write_U32(0x2c100000, codeAddress + 8);
    manager.Write_U32(0x4182001c, codeAddress + 12);
    manager.Write_U32(0x3e000006, codeAddress + 16);
    manager.Write_U32(0x6210059c, codeAddress + 20);
    manager.Write_U32(0x7e107850, codeAddress + 24);
    manager.Write_U32(0x7e0803a6, codeAddress + 28);
    manager.Write_U32(0x4e800021, codeAddress + 32);
    manager.Write_U32(0x48000010, codeAddress + 36);
    manager.Write_U32(0x8a030000, codeAddress + 40);
    manager.Write_U32(0x2c100080, codeAddress + 44);
    manager.Write_U32(0x4080ffe0, codeAddress + 48);
    manager.Write_U32(0x7de803a6, codeAddress + 52);
    manager.Write_U32(0x4e800020, codeAddress + 56);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(codeAddress, 60, true);

    // Branch to this
    manager.Write_U32(0x48152661,referencePointer + 0x83d40);

    Core::System::GetInstance().GetJitInterface().InvalidateICache(referencePointer + 0x83d40, 4, true);
}
