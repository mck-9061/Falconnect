#include "FalconnectSocketManager.h"

#include "ClientState.h"
#include "SFML/System/String.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <wS2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
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

        inet_pton(AF_INET, "localhost", &serverAddress.sin_addr); // Remote IP address

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
}

void FalconnectSocketManager::SendFrame(const RacerMemoryBlock* frame) {
  while (lockFrameToSend)
  {
  }
  lockFrameToSend = true;
    frameToSend = frame;
  lockFrameToSend = false;
    doneFirst = true;
}

void FalconnectSocketManager::SocketThread() {
    Start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    while (shouldRun && !isError) {
        if (shouldDisconnect) {
            break;
        }

        // Read data from server
        char buffer[7680] = { 0 };
        recv(serverSocket, buffer, sizeof(buffer), 0);

        switch (static_cast<FromServerPacketType>(buffer[0])) {
            case FromServerPacketType::CONNECTED: {
              if (buffer[1] == 0)
              {
                //INFO_LOG_FMT(FALCONNECT, "Dropping invalid packet!");
                break;
              }

                hasProperlyConnected = true;
                playerNumber = buffer[1];

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
                char data[256];
                data[0] = static_cast<char>(ToServerPacketType::SETTINGS);
                data[1] = static_cast<char>(racerId);
                data[2] = static_cast<char>(selectedCourse);

                send(serverSocket, data, sizeof(data), 0);

                // Send our name
                char data1[256];
                data1[0] = static_cast<char>(ToServerPacketType::NAME);

                for (int i = 1; i <= 32; i++) {
                    data1[i] = static_cast<char>(name[i]);
                }

                send(serverSocket, data1, sizeof(data1), 0);

                char data2[256];
                data2[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
                data2[1] = static_cast<char>(ClientState::READY);

                send(serverSocket, data2, sizeof(data2), 0);

                break;
            }

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

                char data[256];
                data[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
                data[1] = static_cast<char>(ClientState::GRIDDED);

                send(serverSocket, data, sizeof(data), 0);

                break;
            }

            case FromServerPacketType::START_RACE: {
                INFO_LOG_FMT(FALCONNECT, "START_RACE");
                start = true;

                std::this_thread::sleep_for(std::chrono::seconds(1));

                // Send the first frame
                lockFrameToSend = true;

                INFO_LOG_FMT(FALCONNECT, "Reading data");
                const std::vector<u8> dataToSend = frameToSend->GetSocketData();
                INFO_LOG_FMT(FALCONNECT, "Data read");

                lockFrameToSend = false;

                char data[256];
                data[0] = static_cast<char>(ToServerPacketType::FULL_DATA);

                std::memcpy(data + 1, dataToSend.data(), dataToSend.size());

                send(serverSocket, data, sizeof(data), 0);

                std::this_thread::sleep_for(std::chrono::milliseconds(25));

                break;
            }

            case FromServerPacketType::FULL_DATA: {
                INFO_LOG_FMT(FALCONNECT, "DATA_FULL");
                INFO_LOG_FMT(FALCONNECT, "Player number: {}", playerNumber);
                // Set last read frame
                if (timeBeforePing != 0) ping = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - timeBeforePing;
                INFO_LOG_FMT(FALCONNECT, "Receiving...");

                for (u8 i = 0; i < 30; i++) {
                    u8 usedIndex = i;
                    if (i == 0) usedIndex = playerNumber - 1;
                    if (i == playerNumber - 1) {
                      INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);
                        continue; // Skip our data
                    }

                    INFO_LOG_FMT(FALCONNECT, "Storing racer at index {} in slot {}", i, usedIndex);

                    const std::vector<u8> racerData(buffer + 1 + (i * 255), buffer + 1 + ((i + 1) * 255));

                    if (racerData[0] == 0x00) {
                        INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Invalid data", i);
                    } else {
                        RacerMemoryBlock* block = RacerMemoryBlock::CreateFromSocketData(racerData);
                        operationQueue.push(OperationType::SET_RACER_BLOCK);
                        operationArgumentsQueue.emplace(usedIndex);
                        operationArgumentsQueue.emplace(*block);
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

                    char data[256];
                    data[0] = static_cast<char>(ToServerPacketType::RESET);

                    send(serverSocket, data, sizeof(data), 0);

                    break;
                }

                if (frameToSend != nullptr) {
                    INFO_LOG_FMT(FALCONNECT, "Sending...");

                    // Send frame to be sent to remote
                    timeBeforePing = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                    lockFrameToSend = true;

                    const std::vector<u8> dataToSend = frameToSend->GetSocketData();

                    lockFrameToSend = false;

                    char data[256];
                    data[0] = static_cast<char>(FromServerPacketType::FULL_DATA);

                    std::memcpy(data + 1, dataToSend.data(), dataToSend.size());

                    send(serverSocket, data, sizeof(data), 0);
                }

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

            default:
                break;
        }
    }

    if (!isError) {
        char data[256];
        data[0] = static_cast<char>(ToServerPacketType::DISCONNECT);

        send(serverSocket, data, sizeof(data), 0);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    instance = nullptr;
    delete this;
}
