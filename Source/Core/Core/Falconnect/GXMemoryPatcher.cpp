#include "GXMemoryPatcher.h"

#include "FalconnectSocketManager.h"
#include "GXMemoryReader.h"
#include "PowerPCScripts.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/PowerPC.h"

GXMemoryPatcher::GXMemoryPatcher(const Core::CPUThreadGuard &cpuGuard) : guard(cpuGuard),
                                                                         interface(
                                                                             Core::System::GetInstance().GetPowerPC().
                                                                             GetDebugInterface()) {
    memoryReader = new GXMemoryReader(guard);
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

void GXMemoryPatcher::DisableOptionsMenuControl() const {
    const u32 menuControlAddress = referencePointer + 0x3e28f0;
    interface.SetPatch(guard, menuControlAddress, 0x3c608060);
    interface.SetPatch(guard, menuControlAddress + 4, 0x60000000);
    interface.SetPatch(guard, menuControlAddress + 8, 0x88630000);
}

void GXMemoryPatcher::FullyDisableMenuControl() const {
    const u32 address = referencePointer + 0x3e2604;
    interface.SetPatch(guard, address, 0x60000000);
}

void GXMemoryPatcher::ReEnableMenuControl() const {
    const u32 address = referencePointer + 0x3e2604;
    interface.SetPatch(guard, address, 0x480056d1);
}

void GXMemoryPatcher::SetPracticeModeText(std::string text) const {
    const u32 pointerAddress = referencePointer + 0x406f8c;
    const u32 textPosition = 0x805cf700;
    std::vector<u32> ascii = stringToUint32Array(text);

    // Blank any existing text
    for (int offset = 0; offset < 100; offset++) {
        const u32 address = textPosition + (offset * 4);
        interface.SetPatch(guard, address, 0x0);
    }

    for (u8 offset = 0; offset < ascii.size(); offset++) {
        const u32 address = textPosition + (offset * 4);
        interface.SetPatch(guard, address, ascii[offset]);
    }

    // Replace pointer
    interface.SetPatch(guard, pointerAddress, textPosition);
}

void GXMemoryPatcher::DisableAIControl() const {
    const u32 aiMoveAddress = referencePointer + 0x839a0;

    interface.SetPatch(guard, aiMoveAddress, 0x60000000);
}

void GXMemoryPatcher::DisableCountdown() const {
    const u32 countdownAddress = referencePointer + 0x34a30;

    interface.SetPatch(guard, countdownAddress, 0x48000150);
}

void GXMemoryPatcher::InitialiseText() const {
    // Add text function to unused section of memory
    constexpr u32 functionBaseAddress = 0x80400000;

    interface.SetPatch(guard, functionBaseAddress, PowerPCScripts::CustomStringScript);

    // Create entry point for text function
    const u32 entryPointAddress = referencePointer + 0xca44c;

    const u32 offsetFromBase = functionBaseAddress - entryPointAddress;
    const u32 instruction = 0x48000000 + offsetFromBase + 1;

    interface.SetPatch(guard, entryPointAddress, instruction);

    // // Add text
    // for (int offset = 0; offset < 11; offset++) {
    //     constexpr u32 textAddress = 0x80390000;
    //     const u32 address = textAddress + (offset * 4);
    //     interface.SetPatch(guard, address, PowerPCScripts::InitialText[offset]);
    // }
    SetRenderedText("Falconnect C++ Port Test | Do a barrel roll!");

    // Set text position
    constexpr u32 positionFloatsAddress = 0x80430000;

    constexpr float x = 320;
    constexpr float y = 5;
    constexpr auto bitsX = std::bit_cast<uint32_t>(x);
    constexpr auto bitsY = std::bit_cast<uint32_t>(y);

    interface.SetPatch(guard, positionFloatsAddress, bitsX);
    interface.SetPatch(guard, positionFloatsAddress + 8, bitsY);
}

void GXMemoryPatcher::SetBoostLap(const u8 lap) const {
    interface.SetPatch(guard, referencePointer + 0x3294c, 0x60000000);
    // NOP check for practice mode on lap 2 boost announcement
    interface.SetPatch(guard, referencePointer + 0x34b68, 0x60000000); // NOP announcement at start of practice mode
    interface.SetPatch(guard, referencePointer + 0x33300, 0x60000000); // NOP practice mode check in boost

    interface.SetPatch(guard, referencePointer + 0xc91ec, 0x4800017c);
    // Nullify check for practice mode in energy bar render

    interface.SetPatch(guard, referencePointer + 0x3330C, 0x2c000000 + lap); // Set lap check for boost
    interface.SetPatch(guard, referencePointer + 0xc91d8, 0x2c000001 + lap); // Set lap check for energy bar render
    interface.SetPatch(guard, referencePointer + 0x32938, 0x281e0000 + lap); // Set lap check for announcement
}

void GXMemoryPatcher::StartRaceFromPracticeOptions() const {
    interface.SetPatch(guard, 0x80600000, 0x01010101);
}

void GXMemoryPatcher::StartCountdown() const {
    const u32 countdownAddress = referencePointer + 0x34a30;

    interface.SetPatch(guard, countdownAddress, 0x40820150);
}

void GXMemoryPatcher::SetOpponentRacerIds(const u8 racerIDs[]) const {
    const u32 idLoadAddress = referencePointer + 0x00034E40;

    // Replace instruction to load the correct racer ID to always load given ID
    //interface.SetPatch(guard, idLoadAddress, 0x3AA00000 + racerID);

    // Load array into memory
    for (int i = 0; i < 31; i++) {
        const u32 address = 0x803d1a00 + i;
        if (i == 0) {
            interface.SetPatch(guard, address, 0); // index
            continue;
        }

        u32 data = static_cast<u32>(racerIDs[i - 1]) << 24;
        if (data == 0) data = 0x06000000;

        interface.SetPatch(guard, address, data);
    }

    // Create function to read IDs from array
    constexpr u32 functionAddress = 0x803d1aa0;

    interface.SetPatch(guard, functionAddress, 0x3dc0803d);
    interface.SetPatch(guard, functionAddress + 4, 0x61cf1a00);
    interface.SetPatch(guard, functionAddress + 8, 0x8a0f0000);
    interface.SetPatch(guard, functionAddress + 12, 0x3a300001);
    interface.SetPatch(guard, functionAddress + 16, 0x9a2f0000);
    interface.SetPatch(guard, functionAddress + 20, 0x7eaf88ae);
    interface.SetPatch(guard, functionAddress + 24, 0x39c00000);
    interface.SetPatch(guard, functionAddress + 28, 0x61cf0000);
    interface.SetPatch(guard, functionAddress + 32, 0x7dee7b78);
    interface.SetPatch(guard, functionAddress + 36, 0x7df07b78);
    interface.SetPatch(guard, functionAddress + 40, 0x7df17b78);
    interface.SetPatch(guard, functionAddress + 44, 0x4e800020);

    // Set jump instruction
    interface.SetPatch(guard, idLoadAddress, 0x48000001 + (functionAddress - idLoadAddress));

    const u32 racer_check_address = idLoadAddress + 60;

    // Remove duplicate racer check
    interface.SetPatch(guard, racer_check_address, 0x60000000);
    interface.SetPatch(guard, racer_check_address + 8, 0x60000000);
}

void GXMemoryPatcher::SetRenderedText(const std::string &text) const {
    // Blank previous text
    INFO_LOG_FMT(FALCONNECT, "Blanking text");
    for (int offset = 0; offset < 100; offset++) {
        constexpr u32 textAddress = 0x80390000;
        const u32 address = textAddress + (offset * 4);
        interface.SetPatch(guard, address, 0x0);
    }

    INFO_LOG_FMT(FALCONNECT, "Setting text");
    const std::vector<u32> rep = stringToUint32Array(text);

    for (u8 offset = 0; offset < rep.size(); offset++) {
        constexpr u32 textAddress = 0x80390000;
        const u32 address = textAddress + (offset * 4);
        interface.SetPatch(guard, address, rep[offset]);
    }
}

void GXMemoryPatcher::SetDefaultRaceSettings() const {
    SetSingleByte(referencePointer + 0x24550d, 0x04); // 4 CPU
    SetSingleByte(referencePointer + 0x245517, 0x01); // Allow restore
    SetSingleByte(referencePointer + 0x24551b, 0x03); // 3 laps
    SetSingleByte(referencePointer + 0x2453e9, 0x03); // Master
    SetSingleByte(referencePointer + 0x2453eb, 0x03); // Master
}

void GXMemoryPatcher::SetCpuCount(const u8 cpuCount) const {
    SetSingleByte(referencePointer + 0x24550d, cpuCount);
}

void GXMemoryPatcher::SetSingleByte(const u32 address, const u8 byte) const {
    const u32 mem = interface.ReadMemory(guard, address);

    const u32 read = ((mem << 8) >> 8) | (static_cast<u32>(byte) << 24); // 10 cpus

    interface.SetPatch(guard, address, read);
}

void GXMemoryPatcher::SetRacerData(const u8 racerNum, const RacerMemoryBlock &patchData) const {
  const u32 baseAddress = interface.ReadMemory(guard, referencePointer + 0x227878) + (racerNum * 0x620);

    INFO_LOG_FMT(FALCONNECT, "Base racer address: 0x{}", std::format("{:x}", baseAddress));

    if (baseAddress < 0x80000000) {
        INFO_LOG_FMT(FALCONNECT, "Invalid address!");
        return;
    }

  interface.SetPatch(guard, baseAddress, patchData.state);
    // Make sure the game thinks it's an AI so it isn't trying to update the inputs
    SetSingleByte(baseAddress, 0x84);

  interface.SetPatch(guard, baseAddress + (31 * 4), patchData.centerPosition[0]);
  interface.SetPatch(guard, baseAddress + (32 * 4), patchData.centerPosition[1]);
  interface.SetPatch(guard, baseAddress + (33 * 4), patchData.centerPosition[2]);

  interface.SetPatch(guard, baseAddress + (34 * 4), patchData.lastCenterPosition[0]);
  interface.SetPatch(guard, baseAddress + (35 * 4), patchData.lastCenterPosition[1]);
  interface.SetPatch(guard, baseAddress + (36 * 4), patchData.lastCenterPosition[2]);

  interface.SetPatch(guard, baseAddress + (37 * 4), patchData.velocityWorld[0]);
  interface.SetPatch(guard, baseAddress + (38 * 4), patchData.velocityWorld[1]);
  interface.SetPatch(guard, baseAddress + (39 * 4), patchData.velocityWorld[2]);

  interface.SetPatch(guard, baseAddress + (46 * 4), patchData.velocityMachine[0]);
  interface.SetPatch(guard, baseAddress + (47 * 4), patchData.velocityMachine[1]);
  interface.SetPatch(guard, baseAddress + (48 * 4), patchData.velocityMachine[2]);
    interface.SetPatch(guard, baseAddress + 0xd4, patchData.velocityMachine[2]);

  interface.SetPatch(guard, baseAddress + (59 * 4), patchData.orientationWorld[0]);
  interface.SetPatch(guard, baseAddress + (60 * 4), patchData.orientationWorld[1]);
  interface.SetPatch(guard, baseAddress + (61 * 4), patchData.orientationWorld[2]);

  interface.SetPatch(guard, baseAddress + (63 * 4), patchData.upVector[0]);
  interface.SetPatch(guard, baseAddress + (64 * 4), patchData.upVector[1]);
  interface.SetPatch(guard, baseAddress + (65 * 4), patchData.upVector[2]);

  interface.SetPatch(guard, baseAddress + (67 * 4), patchData.orientationGravity[0]);
  interface.SetPatch(guard, baseAddress + (68 * 4), patchData.orientationGravity[1]);
  interface.SetPatch(guard, baseAddress + (69 * 4), patchData.orientationGravity[2]);

  interface.SetPatch(guard, baseAddress + (95 * 4), patchData.speed);
  interface.SetPatch(guard, baseAddress + (96 * 4), patchData.arialTilt);
  interface.SetPatch(guard, baseAddress + (97 * 4), patchData.energy);

  interface.SetPatch(guard, baseAddress + (111 * 4), patchData.trackOrientation[0]);
  interface.SetPatch(guard, baseAddress + (112 * 4), patchData.trackOrientation[1]);
  interface.SetPatch(guard, baseAddress + (113 * 4), patchData.trackOrientation[2]);

  interface.SetPatch(guard, baseAddress + (117 * 4), patchData.bottomPosition[0]);
  interface.SetPatch(guard, baseAddress + (118 * 4), patchData.bottomPosition[1]);
  interface.SetPatch(guard, baseAddress + (119 * 4), patchData.bottomPosition[2]);

  interface.SetPatch(guard, baseAddress + (123 * 4), patchData.inputs[0]);
  interface.SetPatch(guard, baseAddress + (124 * 4), patchData.inputs[1]);
  interface.SetPatch(guard, baseAddress + (125 * 4), patchData.inputs[2]);
    INFO_LOG_FMT(FALCONNECT, "Accelerator input: {}", patchData.inputs[5]);
    interface.SetPatch(guard, baseAddress + (126 * 4), patchData.inputs[3]);
    interface.SetPatch(guard, baseAddress + (127 * 4), patchData.inputs[4]);
    interface.SetPatch(guard, baseAddress + (128 * 4), patchData.inputs[5]);
    interface.SetPatch(guard, baseAddress + (129 * 4), patchData.inputs[6]);

  interface.SetPatch(guard, baseAddress + (388 * 4), patchData.sideAttack);
}

void GXMemoryPatcher::SetRacerMachineName(const u8 racerNum, const std::vector<u8> &name) const {
    const u32 baseAddress = interface.ReadMemory(guard, referencePointer + 0x227878) + (racerNum * 0x620);

    INFO_LOG_FMT(FALCONNECT, "Base racer address: 0x{}", std::format("{:x}", baseAddress));

    if (baseAddress < 0x80000000) {
        INFO_LOG_FMT(FALCONNECT, "Invalid address!");
        return;
    }

    interface.SetPatch(guard, baseAddress + 0x3c, name);
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
    //     interface.SetPatch(guard, address + (i * 8), loadInstruction);
    //     interface.SetPatch(guard, address + (i * 8) + 4, storeInstruction);
    // }
    //
    // interface.SetPatch(guard, address + (30 * 8), 0x39c00000); // Reset r14
    // interface.SetPatch(guard, address + (30 * 8) + 4, 0x4e800020); // Return

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

        interface.SetPatch(guard, 0x80376a00 + (i * 8), loadInstruction);
        interface.SetPatch(guard, 0x80376a00 + (i * 8) + 4, storeInstruction);
    }

    interface.SetPatch(guard, 0x80376a00 + (30 * 8), 0x39c00000); // Reset r14
    interface.SetPatch(guard, 0x80376a00 + (30 * 8) + 4, 0x4e800020); // Return

    interface.SetPatch(guard, address, jumpInstruction);
}

void GXMemoryPatcher::SetCourse(const u8 courseID) const {
    const u32 address = referencePointer + 0x245471;
    SetSingleByte(address, courseID);
}

void GXMemoryPatcher::ConstrainMenu() const {
    const u32 address = referencePointer + 0x1bf144;

    if (const u8 current = memoryReader->Read8(0x1bf144); current == 3 || current == 5) {
        SetSingleByte(address, 6);
    }
}

void GXMemoryPatcher::InitialiseNameLabels() const {
    // Set font
    SetSingleByte(referencePointer + 0x12a217, 0x03);

    // Force all drivers to be rivals
    interface.SetPatch(guard, referencePointer + 0x129bc4, 0x3ae00001);

    // Branch to new code to load pointer
    interface.SetPatch(guard, referencePointer + 0x129c3c, 0x48092d65);
    interface.SetPatch(guard, referencePointer + 0x129c88, 0x48092d19);
    interface.SetPatch(guard, referencePointer + 0x129ce4, 0x48092cbd);
    interface.SetPatch(guard, referencePointer + 0x129d10, 0x48092c91);

    // New code: Load pointer into r5
    // pointer = base pointer + (0x20 * r27)
    // 1d 1b 00 20
    // 3c a0 80 37
    // 60 a5 7d 00
    // 7c a5 42 14
    // 39 00 00 00
    // 4e 80 00 20

    const u32 codeAddress = referencePointer + 0x1bc9a0;

    interface.SetPatch(guard, codeAddress, 0x1d1b0020);
    interface.SetPatch(guard, codeAddress + 4, 0x3ca08037);
    interface.SetPatch(guard, codeAddress + 8, 0x60a57d00);
    interface.SetPatch(guard, codeAddress + 12, 0x7ca54214);
    interface.SetPatch(guard, codeAddress + 16, 0x39000000);
    interface.SetPatch(guard, codeAddress + 20, 0x4e800020);

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
        interface.SetPatch(guard, nameAddress, FalconnectSocketManager::instance->names[i]);
        SetRacerMachineName(usedIndex, FalconnectSocketManager::instance->names[i]);
    }
}

void GXMemoryPatcher::ResetToTitle() const {
    interface.SetPatch(guard, referencePointer + 0x245474, 0x24000100);
}
