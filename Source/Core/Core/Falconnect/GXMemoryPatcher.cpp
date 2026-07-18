#include "GXMemoryPatcher.h"

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

void GXMemoryPatcher::DisableMenuControl() const {
    const u32 menuControlAddress = referencePointer + 0x3e28f0;
    interface.SetPatch(guard, menuControlAddress, 0x3c608060);
    interface.SetPatch(guard, menuControlAddress + 4, 0x60000000);
    interface.SetPatch(guard, menuControlAddress + 8, 0x88630000);
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

void GXMemoryPatcher::SetOpponentRacerId(const u8 racerID) const {
    const u32 idLoadAddress = referencePointer + 0x00034E40;

    // Replace instruction to load the correct racer ID to always load given ID
    interface.SetPatch(guard, idLoadAddress, 0x3AA00000 + racerID);

    const u32 racer_check_address = idLoadAddress + 60;

    // Remove duplicate racer check
    interface.SetPatch(guard, racer_check_address, 0x60000000);
    interface.SetPatch(guard, racer_check_address + 8, 0x60000000);
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
    SetSingleByte(referencePointer + 0x24550d, 0x01); // 1 CPU
    SetSingleByte(referencePointer + 0x245517, 0x00); // No restore
    SetSingleByte(referencePointer + 0x24551b, 0x01); // 1 lap
    SetSingleByte(referencePointer + 0x2453e9, 0x03); // Master
    SetSingleByte(referencePointer + 0x2453eb, 0x03); // Master
}

void GXMemoryPatcher::SetSingleByte(u32 address, u8 byte) const {
    const u32 mem = interface.ReadMemory(guard, address);

    const u32 read = ((mem << 8) >> 8) | (static_cast<u32>(byte) << 24); // 10 cpus

    interface.SetPatch(guard, address, read);
}

void GXMemoryPatcher::SetRacerData(u8 racerNum, const RacerMemoryBlock &patchData) const {
  const u32 baseAddress = interface.ReadMemory(guard, referencePointer + 0x227878) + (racerNum * 0x620);

    INFO_LOG_FMT(FALCONNECT, "Base racer address: 0x{}", std::format("{:x}", baseAddress));

    if (baseAddress < 0x80000000) {
        INFO_LOG_FMT(FALCONNECT, "Invalid address!");
        return;
    }

  interface.SetPatch(guard, baseAddress, patchData.state);

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

  interface.SetPatch(guard, baseAddress + (388 * 4), patchData.sideAttack);
}
