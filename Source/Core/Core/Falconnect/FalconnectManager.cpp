//
// Created by Nathaniel Carter on 31/03/2026.
//

#include "FalconnectManager.h"

#include <iostream>
#include <thread>

#include "Core/System.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PowerPC.h"

[[noreturn]] void TestCheckMem() {
    while (true) {
        INFO_LOG_FMT(COMMON, "Falconnect test");
        auto& system = Core::System::GetInstance();

        if (system.GetCPU().GetState() == CPU::State::Running) {
            PPCDebugInterface& interface = system.GetPowerPC().GetDebugInterface();

            u32 address = 0x800030c8;
            u32 read = interface.ReadMemory(*FalconnectManager::guard, address);

            std::stringstream stream;
            stream << std::hex << read;
            INFO_LOG_FMT(COMMON, "[Falconnect] Read: {}", stream.str());
        } else {
            INFO_LOG_FMT(COMMON, "Not running!");
        }

        std::this_thread::sleep_for(std::chrono::nanoseconds(1000000000));
    }
}

Core::CPUThreadGuard* FalconnectManager::guard;

void FalconnectManager::StartThread() {
    guard = new Core::CPUThreadGuard(Core::System::GetInstance());
    //std::thread(TestCheckMem).detach();
}