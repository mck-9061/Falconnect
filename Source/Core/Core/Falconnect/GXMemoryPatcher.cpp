#include "GXMemoryPatcher.h"

#include "GXMemoryReader.h"
#include "PowerPCScripts.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/PowerPC.h"

GXMemoryPatcher::GXMemoryPatcher(const Core::CPUThreadGuard& cpuGuard) :
guard(cpuGuard),
interface(Core::System::GetInstance().GetPowerPC().GetDebugInterface())
{
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

void GXMemoryPatcher::DisableAIControl() {
}

void GXMemoryPatcher::DisableCountdown() {
}

void GXMemoryPatcher::InitialiseText() const {
    // Add text function to unused section of memory
    constexpr u32 functionBaseAddress = 0x80400000;

    interface.SetPatch(guard, functionBaseAddress, PowerPCScripts::CustomStringScript);

    // Create entry point for text function
    const u32 entryPointAddress = referencePointer + 0xca44c;

    const u32 offset = functionBaseAddress - entryPointAddress;
    const u32 instruction = 0x48000000 + offset + 1;

    interface.SetPatch(guard, entryPointAddress, instruction);

    // Add text
    for (int offset = 0; offset < 11; offset++) {
        constexpr u32 textAddress = 0x80390000;
        const u32 address = textAddress + (offset * 4);
        interface.SetPatch(guard, address, PowerPCScripts::InitialText[offset]);
    }

    // Set text position
    constexpr u32 positionFloatsAddress = 0x80430000;

    constexpr float x = 320;
    constexpr float y = 5;
    constexpr auto bitsX = std::bit_cast<uint32_t>(x);
    constexpr auto bitsY = std::bit_cast<uint32_t>(y);

    interface.SetPatch(guard, positionFloatsAddress, bitsX);
    interface.SetPatch(guard, positionFloatsAddress + 8, bitsY);
}

void GXMemoryPatcher::SetBoostLap() const {
    interface.SetPatch(guard, referencePointer + 0x3294c, 0x60000000); // NOP check for practice mode on lap 2 boost announcement
    interface.SetPatch(guard, referencePointer + 0x34b68, 0x60000000); // NOP announcement at start of practice mode
    interface.SetPatch(guard, referencePointer + 0x33300, 0x60000000); // NOP practice mode check in boost

    interface.SetPatch(guard, referencePointer + 0xc91ec, 0x4800017c); // Nullify check for practice mode in energy bar render

    interface.SetPatch(guard, referencePointer + 0x3330C, 0x2c000002); // Set lap check for boost
    interface.SetPatch(guard, referencePointer + 0xc91d8, 0x2c000003); // Set lap check for energy bar render
    interface.SetPatch(guard, referencePointer + 0x32938, 0x281e0002); // Set lap check for announcement
}
