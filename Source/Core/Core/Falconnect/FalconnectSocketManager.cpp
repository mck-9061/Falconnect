#include "FalconnectSocketManager.h"

#include "ClientState.h"
#include "Common/Assert.h"
#include "SFML/System/String.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <wS2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "expr.h"
#endif

#include <chrono>
#include <thread>

#include "FalconnectManager.h"
#include "PacketType.h"
#include "Common/Logging/Log.h"

FalconnectSocketManager* FalconnectSocketManager::instance = nullptr;

void FalconnectSocketManager::Start() {
    // Connect to the server
    INFO_LOG_FMT(FALCONNECT, "Connecting...");
    int code = -1;

    for (u8 attempt = 1; attempt <= 10; attempt++) {
        INFO_LOG_FMT(FALCONNECT, "Attempt {}", attempt);
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(8000);

        inet_pton(AF_INET, "162.19.231.212", &serverAddress.sin_addr); // Remote IP address

        code = connect(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress));
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (code != -1) break;
    }
    INFO_LOG_FMT(FALCONNECT, "Code: {}", code);

    if (code < 0) {
        isError = true;
    } else {
        hasConnected = true;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void FalconnectSocketManager::SendFrame(RacerMemoryBlock *frame, const u8 index) {
  while (lockFrameToSend)
  {
  }
  lockFrameToSend = true;
    framesToSend[index] = frame;
  lockFrameToSend = false;
    doneFirst = true;
}

bool recvAll(int sock, char* buffer, int size)
{
    int total = 0;

    while (total < size)
    {
        int n = recv(sock, buffer + total, size - total, 0);

        if (n <= 0)
            return false;

        total += n;
    }

    return true;
}

void FalconnectSocketManager::SocketThread() {
    Start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    int invalidPacketCount = 0;

    while (shouldRun && !isError) {
        if (shouldDisconnect) {
            break;
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(4));
        // Read data from server
        char buffer[3844] = { 0 };
        recvAll(serverSocket, buffer, sizeof(buffer));

        switch (static_cast<FromServerPacketType>(buffer[0])) {
            case FromServerPacketType::DISCONNECT: {
                INFO_LOG_FMT(FALCONNECT, "Disconnecting!");
                hasProperlyConnected = true;
                auto reason = std::string(buffer + 1);
                SuccessAlertFmt("You have been disconnected from the Falconnect server.\n"
                                "Reason: {}", reason);

                FalconnectManager::instance->shouldReset = true;
                shouldRun = false;
                shouldDisconnect = true;
                isError = true;
                break;
            }

            case FromServerPacketType::CONNECTED: {
              if (buffer[1] == 0)
              {
                INFO_LOG_FMT(FALCONNECT, "Dropping invalid packet!");
                  invalidPacketCount++;

                  if (invalidPacketCount > 50) {
                      // Server probably closed, or we lost connection
                      INFO_LOG_FMT(FALCONNECT, "Disconnecting!");
                      {
                          FalconnectManager::instance->shouldDisplayDisconnectedAlert = true;
                      }
                      std::this_thread::sleep_for(std::chrono::seconds(1));
                      shouldDisconnect = true;
                      shouldRun = false;
                      isError = true;
                      FalconnectManager::instance->shouldReset = true;
                  }
                break;
              }

                hasProperlyConnected = true;
                playerNumber = buffer[1];
                ourCpus = buffer[2];
                cpuStartIndex = buffer[3];

                // Setup UDP
#ifdef _WIN32
                closesocket(serverUdpSocket);
#else
                close(serverUdpSocket);
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                serverUdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

                serverUdpAddress.sin_family = AF_INET;
                serverUdpAddress.sin_port = htons(9000 - playerNumber);

                inet_pton(AF_INET, "162.19.231.212", &serverUdpAddress.sin_addr); // Remote IP address

                // connect(serverUdpSocket, reinterpret_cast<struct sockaddr *>(&serverUdpAddress), sizeof(serverUdpAddress));

                // Send our name
                char data1[3844];
                data1[0] = static_cast<char>(ToServerPacketType::NAME);

                for (int i = 1; i <= 32; i++) {
                    data1[i] = static_cast<char>(name[i]);
                }

                send(serverSocket, data1, 124 * (1 + ourCpus) + 5, 0);

                // Connected: Wait for us to be ready
                while (!canLoad) {
                    INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    if (shouldDisconnect) {
                        break;
                    }
                }

                if (shouldDisconnect) {
                    break;
                }

                // First send our selected racer ID, then say we're ready
                char data[3844];
                data[0] = static_cast<char>(ToServerPacketType::SETTINGS);
                data[1] = static_cast<char>(racerId);
                data[2] = static_cast<char>(selectedCourse);

                send(serverSocket, data, 124 * (1 + ourCpus) + 5, 0);

                char data2[3844];
                data2[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
                data2[1] = static_cast<char>(ClientState::READY);
                data2[2] = 1;

                send(serverSocket, data2, 124 * (1 + ourCpus) + 5, 0);

                break;
            }

            default:
                invalidPacketCount = 0;

            // case FromServerPacketType::RACER_ID: {
            //     INFO_LOG_FMT(FALCONNECT, "RACER_ID");
            //     exited = false;
            //     FalconnectManager::instance->racerIDs[1] = buffer[1];
            //
            //     if (!isHost) {
            //         // Send back our own racer ID
            //         char data[256];
            //         data[0] = static_cast<char>(FromServerPacketType::RACER_ID);
            //         data[1] = FalconnectManager::instance->racerIDs[0];
            //
            //         send(serverSocket, data, sizeof(data), 0);
            //     } else {
            //         // Both players have racer IDs, so start the race
            //         char data[256];
            //         data[0] = static_cast<char>(FromServerPacketType::START_RACE);
            //
            //         send(serverSocket, data, sizeof(data), 0);
            //         FalconnectManager::instance->shouldStart = true;
            //     }
            //
            //     break;
            // }

            case FromServerPacketType::COURSE: {
                usedCourseId = buffer[1];
                cpuCount = buffer[2];

                break;
            }

            case FromServerPacketType::RACER_IDS: {
                // Construct array
                for (u8 i = 0; i < 30; i++) {
                    u8 usedIndex = i;
                    if (i == 0) usedIndex = playerNumber - 1;
                    if (i == playerNumber - 1) {
                        INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);
                        continue; // Skip our data
                    }

                    const char racerNum = buffer[i + 1];
                    FalconnectManager::instance->racerIDs[usedIndex - 1] = racerNum;
                }

                break;
            }

            case FromServerPacketType::NAMES: {
                names.clear();

                for (int i = 0; i < 30; i++) {
                    // char name[32];
                    // std::memcpy(name, buffer + 1 + (i * 32), 32);
                    //
                    // std::vector<u8> iHateCpp{};
                    //
                    // iHateCpp.reserve(32);
                    // for (const char j : name) {
                    //     iHateCpp.push_back(j);
                    // }

                    const std::vector<u8> iHateCpp(buffer + 1 + (i * 32), buffer + 1 + ((i + 1) * 32));

                    std::string str(iHateCpp.begin(), iHateCpp.end());
                    INFO_LOG_FMT(FALCONNECT, "Received name: {}", str);

                    names.push_back(iHateCpp);
                }

                break;
            }

            case FromServerPacketType::START: {
                INFO_LOG_FMT(FALCONNECT, "START");
                exited = false;

                { // force the compiler to not be a whiny baby
                  FalconnectManager::instance->shouldStart = true;
                }

                std::this_thread::sleep_for(std::chrono::seconds(5));

                // Tell the server when we've gridded
                while (!hasGridded) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    if (shouldDisconnect) {
                        break;
                    }
                }

                if (shouldDisconnect) {
                    break;
                }

                char data[3844];
                data[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
                data[1] = static_cast<char>(ClientState::GRIDDED);
                data[2] = 1;

                send(serverSocket, data, 124 * (1 + ourCpus) + 5, 0);

                break;
            }

            case FromServerPacketType::START_RACE: {
                INFO_LOG_FMT(FALCONNECT, "START_RACE");
                start = true;

                std::this_thread::sleep_for(std::chrono::seconds(1));

                shouldRunDataThread = true;
                std::thread readThread(&FalconnectSocketManager::DataThread, this);
                readThread.detach();

                std::thread memThread(&FalconnectSocketManager::MemoryThread, this);
                memThread.detach();

                std::thread sendThread(&FalconnectSocketManager::SendDataThread, this);
                sendThread.detach();

                break;
            }

            case FromServerPacketType::FULL_DATA: {


                // std::this_thread::sleep_for(std::chrono::milliseconds(25));
                // }

                break;
            }

            // case FromServerPacketType::RESET: {
            //     hasGridded = false;
            //     start = false;
            //     exited = false;
            //     canLoad = false;
            //
            //     // If we're the host, wait until we can load
            //     if (isHost) {
            //         // ReSharper disable once CppDFAEndlessLoop
            //         while (!canLoad) {
            //             INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
            //             std::this_thread::sleep_for(std::chrono::seconds(1));
            //         }
            //         INFO_LOG_FMT(FALCONNECT, "Waiting for partner...");
            //     } else {
            //         // Wait until we can load, then tell the host we're ready
            //         while (!canLoad) {
            //             INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
            //             std::this_thread::sleep_for(std::chrono::seconds(1));
            //         }
            //
            //         char data[256];
            //         data[0] = static_cast<char>(FromServerPacketType::READY_TO_START);
            //
            //         send(serverSocket, data, sizeof(data), 0);
            //     }
            //
            //     break;
            // }
        }
    }

    if (!isError) {
        char data[3844];
        data[0] = static_cast<char>(ToServerPacketType::DISCONNECT);

        send(serverSocket, data, 124 * (1 + ourCpus) + 5, 0);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    delete this;
}

void FalconnectSocketManager::MemoryThread() const {
    u32 timeBeforeUpdate = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    while (shouldRunDataThread) {
        if (FalconnectManager::instance->currentState == GameState::RACING) {
            for (int i = 0; i < 30; i++) {
                if (const auto racerNum = instance->usedIndices[i]; racerNum != 0) {
                    const auto racerBlock = instance->allBlocks[i];

                    FalconnectManager::instance->patcher->SetRacerData(racerNum, *racerBlock, true);
                    //FalconnectManager::instance->lastWrittenBlocks[racerNum - 1] = racerBlock;
                    //INFO_LOG_FMT(FALCONNECT, "Racer data set");
                }
            }

            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            const u32 time = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            //FalconnectManager::instance->patcher->SetRenderedText(std::to_string(time - timeBeforeUpdate));
            timeBeforeUpdate = time;
        }
    }
}

void FalconnectSocketManager::SendDataThread() {
    u32 sentCount = 0;
    while (shouldRunDataThread) {
        if (FalconnectManager::instance->currentState != GameState::RACING) continue;
        // Send our frames
        sentCount++;

        instance->SendFrame(FalconnectManager::instance->patcher->memoryReader->ReadRacerData(0), 0);

        for (int i = 0; i < instance->ourCpus; i++) {
            //INFO_LOG_FMT(FALCONNECT, "Reading racer at index {}", FalconnectSocketManager::instance->cpuStartIndex + i);
            instance->SendFrame(FalconnectManager::instance->patcher->memoryReader->ReadRacerData(instance->cpuStartIndex + i), i + 1);
        }

        while (framesToSend[0] == nullptr) {
            INFO_LOG_FMT(FALCONNECT, "Bad frame!");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        INFO_LOG_FMT(FALCONNECT, "Sending...");

        // Send frame to be sent to remote
        timeBeforePing = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        lockFrameToSend = true;

        char data[3844];
        data[0] = static_cast<char>(FromServerPacketType::FULL_DATA);

        data[1] = static_cast<char>((sentCount >> 24) & 0xff);
        data[2] = static_cast<char>((sentCount >> 16) & 0xff);
        data[3] = static_cast<char>((sentCount >> 8) & 0xff);
        data[4] = static_cast<char>(sentCount & 0xff);

        int i = 0;
        int j = 1;
        for (const RacerMemoryBlock* frame : framesToSend) {
            if (j > ourCpus + 1) break;

            const std::vector<u8> dataToSend = frame->GetSocketData();

            std::memcpy(data + 5 + i, dataToSend.data(), dataToSend.size());

            i += 124;
            j++;
        }

        lockFrameToSend = false;

        sendto(serverUdpSocket,
            data,
            124 * (1 + ourCpus) + 5,
            0,
            reinterpret_cast<sockaddr *>(&serverUdpAddress),
            sizeof(serverUdpAddress));

        INFO_LOG_FMT(FALCONNECT, "Sent");

        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
}

void FalconnectSocketManager::DataThread() {
    while (shouldRunDataThread) {
        // Receive datagram
        char buffer[3844] = { 0 };
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);

        recvfrom(serverUdpSocket,
                             buffer,
                             sizeof(buffer),
                             0,
                             reinterpret_cast<sockaddr *>(&from),
                             &fromLen);

        // ping spoofing lol
        // std::this_thread::sleep_for(std::chrono::milliseconds(6));

        INFO_LOG_FMT(FALCONNECT, "DATA_FULL");
        INFO_LOG_FMT(FALCONNECT, "Player number: {}", playerNumber);
        // Set last read frame
        if (timeBeforePing != 0) ping = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - timeBeforePing;
        INFO_LOG_FMT(FALCONNECT, "Receiving...");

        const u32 packetNum =
            ((buffer[1] & 0xff) << 24) |
                ((buffer[2] & 0xff) << 16) |
                    ((buffer[3] & 0xff) << 8) |
                        (buffer[4] & 0xff);

        if (packetNum < lastPacketNum) {
            INFO_LOG_FMT(FALCONNECT, "Skipping old packet {}", packetNum);
            continue;
        }

        lastPacketNum = packetNum;
        INFO_LOG_FMT(FALCONNECT, "Packet {}", packetNum);

        for (u8 i = 0; i < 30; i++) {
            u8 usedIndex = i;
            if (i == 0) usedIndex = playerNumber - 1;
            if (i == playerNumber - 1) {
              INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);
                continue; // Skip our data
            }

            if (usedIndex >= cpuStartIndex && usedIndex < cpuStartIndex + ourCpus) {
                INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our CPU", i);
                continue;
            }

            INFO_LOG_FMT(FALCONNECT, "Storing racer at index {} in slot {}", i, usedIndex);

            if (const std::vector<u8> racerData(buffer + 5 + (i * 124), buffer + 5 + ((i + 1) * 124)); racerData[0] == 0x00) {
                INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Invalid data", i);
            } else {
                RacerMemoryBlock* block = RacerMemoryBlock::CreateFromSocketData(racerData);
                // operationQueue.push(OperationType::SET_RACER_BLOCK);
                // operationArgumentsQueue.emplace(usedIndex);
                // operationArgumentsQueue.emplace(*block);

                //if (allBlocks[i] != nullptr) updated[i] = *block != *allBlocks[i];
                //else updated[i] = true;

                allBlocks[i] = block;
                usedIndices[i] = usedIndex;

                //FalconnectManager::instance->patcher->SetRacerData(usedIndex, *block, true);
                //FalconnectManager::instance->lastWrittenBlocks[usedIndex - 1] = block;
            }
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(16));

        // If exited, stop sending frames
        if (exited) {
            // Reset everything, tell server
            hasGridded = false;
            start = false;
            exited = false;
            canLoad = false;
            shouldRunDataThread = false;

            char data[3844];
            data[0] = static_cast<char>(ToServerPacketType::RESET);

            send(serverSocket, data, 124 * (1 + ourCpus) + 5, 0);

            break;
        }
    }
}
