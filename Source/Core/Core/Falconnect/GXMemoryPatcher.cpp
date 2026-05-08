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
    for (int offset = 0; offset < 100; offset++) {
        constexpr u32 textAddress = 0x80390000;
        const u32 address = textAddress + (offset * 4);
        interface.SetPatch(guard, address, 0x0);
    }

    const std::vector<u32> rep = stringToUint32Array(text);

    for (u8 offset = 0; offset < rep.size(); offset++) {
        constexpr u32 textAddress = 0x80390000;
        const u32 address = textAddress + (offset * 4);
        interface.SetPatch(guard, address, rep[offset]);
    }
}
