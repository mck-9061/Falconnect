#include <mutex>
#include <optional>
#include <queue>
#include <Windows.h>

#include "Core/System.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitInterface.h"

namespace IPC
{
  void ServerThread()
  {
    const char* pipe_name = "\\\\.\\pipe\\DolphinIPC";

    HANDLE pipe = CreateNamedPipeA(pipe_name, PIPE_ACCESS_INBOUND,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 1024,
                                   1024, 0, NULL);

    OutputDebugStringA("[IPC] Pipe created\n");

    while (true)
    {
      if (ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED)
      {
        char buffer[256];
        DWORD read;

        if (ReadFile(pipe, buffer, sizeof(buffer) - 1, &read, NULL))
        {
          buffer[read] = '\0';

          uint32_t addr, size;

          if (sscanf(buffer, "invalidate %x %x", &addr, &size) == 2)
          {
            auto& system = Core::System::GetInstance();

            system.GetPowerPC().ScheduleInvalidateCacheThreadSafe(addr);
            //system.GetJitInterface().InvalidateICache(addr, size, false);

            OutputDebugStringA("[IPC] Invalidated\n");
          }
        }

        DisconnectNamedPipe(pipe);
      }
    }
  }

  void Start()
  {
    std::thread(ServerThread).detach();
  }
}  // namespace IPC
