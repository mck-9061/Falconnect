//
// Created by Nathaniel Carter on 31/03/2026.
//

#include "FalconnectManager.h"

#include <iostream>
#include <thread>

#include "Core/System.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PowerPC.h"

FalconnectManager* FalconnectManager::instance = nullptr;

FalconnectManager::FalconnectManager() {
    referencePointer = 0;
    currentState = GameState::NOT_RUNNING;
    instance = this;
}

void FalconnectManager::Update(const Core::CPUThreadGuard &guard) {
    auto& system = Core::System::GetInstance();

    if (system.GetCPU().GetState() != CPU::State::Running) {
        INFO_LOG_FMT(FALCONNECT, "Not running!");
        currentState = GameState::NOT_RUNNING;
        return;
    }

    // If we need to load the reference pointer, do so
    if (referencePointer == 0 || referencePointer & 0x01000000) {
        INFO_LOG_FMT(FALCONNECT, "Loading reference pointer...");
        LoadReferencePointer(guard);

        if (referencePointer == 0 || referencePointer & 0x01000000) {
            INFO_LOG_FMT(FALCONNECT, "Reference pointer invalid or not yet defined!");
            currentState = GameState::WAITING_FOR_REFERENCE;
            return;
        }

        INFO_LOG_FMT(FALCONNECT, "Reference pointer located.");
        currentState = GameState::NOT_IN_PRACTICE;
    }

    // Reference pointer located; wait for practice mode
    u16 mode = Read16(guard, 0x2453e0);

    if (mode != 3) {
        INFO_LOG_FMT(FALCONNECT, "Not in Practice mode!");
        currentState = GameState::NOT_IN_PRACTICE;
        return;
    }

    if (currentState == GameState::NOT_IN_PRACTICE) currentState = GameState::IN_PRACTICE;

    u16 state = Read16(guard, 0x2454e2);

    if (state != 7) {
        INFO_LOG_FMT(FALCONNECT, "Waiting for all settings to be chosen...");
        return;
    }

    if (currentState == GameState::IN_PRACTICE) currentState = GameState::SETTINGS_SELECTED;

    if (currentState == GameState::SETTINGS_SELECTED) {
        INFO_LOG_FMT(FALCONNECT, "Setting up...");
        PPCDebugInterface& interface = system.GetPowerPC().GetDebugInterface();

        // Disable menu control
        u32 menuControlAddress = referencePointer + 0x3e28f0;
        interface.SetPatch(guard, menuControlAddress, 0x3c608060);
        interface.SetPatch(guard, menuControlAddress + 4, 0x60000000);
        interface.SetPatch(guard, menuControlAddress + 8, 0x88630000);

        // Disable AI control

        // Disable countdown

        // Set up waiting text


        currentState = GameState::READY_TO_LOAD;
        return;
    }
}

// Private methods
void FalconnectManager::LoadReferencePointer(const Core::CPUThreadGuard& guard) {
    auto& system = Core::System::GetInstance();

    PPCDebugInterface& interface = system.GetPowerPC().GetDebugInterface();

    u32 address = 0x800030c8;
    referencePointer = interface.ReadMemory(guard, address);

    std::stringstream stream;
    stream << std::hex << referencePointer;
    INFO_LOG_FMT(FALCONNECT, "Read: {}", stream.str());
}

u16 FalconnectManager::Read16(const Core::CPUThreadGuard &guard, u32 offset) {
    auto& system = Core::System::GetInstance();

    PPCDebugInterface& interface = system.GetPowerPC().GetDebugInterface();

    u32 address = referencePointer + offset;
    u32 mem = interface.ReadMemory(guard, address);

    u16 read = static_cast<u8>(mem >> 16);

    return read;
}
