#include "FalconnectSocketManager.h"

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
    if (isHost) {
        localSocket = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in localAddress{};
        localAddress.sin_family = AF_INET;
        localAddress.sin_port = htons(8000);
        localAddress.sin_addr.s_addr = INADDR_ANY;

        // Bind socket
        const int a = bind(localSocket, reinterpret_cast<struct sockaddr *>(&localAddress),
             sizeof(localAddress));
        INFO_LOG_FMT(FALCONNECT, "Bind: {}", a);
        INFO_LOG_FMT(FALCONNECT, "Started Falconnect server on {}:{}. Waiting for connection...", localAddress.sin_addr.s_addr, localAddress.sin_port);

        listen(localSocket, 5);

        INFO_LOG_FMT(FALCONNECT, "Listening...");

        struct sockaddr_in remoteAddress{};
        socklen_t remoteLength;
        remoteLength = sizeof remoteAddress;

        // Accept client
        remoteSocket = accept(localSocket, reinterpret_cast<struct sockaddr *>(&remoteAddress), &remoteLength);
        // new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

        INFO_LOG_FMT(FALCONNECT, "Connection established!");
        return;
    }

    // Not the host; connect to host
    remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in remoteAddress{};
    remoteAddress.sin_family = AF_INET;
    remoteAddress.sin_port = htons(8000);
    inet_pton(AF_INET, "localhost", &remoteAddress.sin_addr); // Remote IP address

    INFO_LOG_FMT(FALCONNECT, "Connecting...");
    connect(remoteSocket, reinterpret_cast<struct sockaddr *>(&remoteAddress), sizeof(remoteAddress));
    INFO_LOG_FMT(FALCONNECT, "Connection established!");
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

    if (isHost) {
        // Start communication
        char data[256];
        data[0] = static_cast<char>(PacketType::RACER_ID);
        data[1] = FalconnectManager::instance->racerIDs[0];

        send(remoteSocket, data, sizeof(data), 0);
    }

    while (shouldRun) {
        // Read data from remote
        char buffer[256] = { 0 };
        recv(remoteSocket, buffer, sizeof(buffer), 0);

        switch (static_cast<PacketType>(buffer[0])) {
            case PacketType::RACER_ID: {
                INFO_LOG_FMT(FALCONNECT, "RACER_ID");
                exited = false;
                FalconnectManager::instance->racerIDs[1] = buffer[1];

                if (!isHost) {
                    // Send back our own racer ID
                    char data[256];
                    data[0] = static_cast<char>(PacketType::RACER_ID);
                    data[1] = FalconnectManager::instance->racerIDs[0];

                    send(remoteSocket, data, sizeof(data), 0);
                } else {
                    // Both players have racer IDs, so start the race
                    char data[256];
                    data[0] = static_cast<char>(PacketType::START_RACE);

                    send(remoteSocket, data, sizeof(data), 0);
                    FalconnectManager::instance->shouldStart = true;
                }

                break;
            }

            case PacketType::START_RACE: {
                INFO_LOG_FMT(FALCONNECT, "START_RACE");
                exited = false;

                {
                  FalconnectManager::instance->shouldStart = true;
                }

                std::this_thread::sleep_for(std::chrono::seconds(5));

                // Tell the host when we've gridded
                while (!hasGridded) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                char data[256];
                data[0] = static_cast<char>(PacketType::READY);

                send(remoteSocket, data, sizeof(data), 0);

                break;
            }

            case PacketType::READY: {
                INFO_LOG_FMT(FALCONNECT, "READY");
                // Wait for us to be gridded, then start the countdown
                while (!hasGridded) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                char data[256];
                data[0] = static_cast<char>(PacketType::COUNTDOWN);

                send(remoteSocket, data, sizeof(data), 0);

                start = true;

                break;
            }

            case PacketType::COUNTDOWN: {
                INFO_LOG_FMT(FALCONNECT, "COUNTDOWN");
                start = true;
                INFO_LOG_FMT(FALCONNECT, "sucessfully set a bool lmao");

                std::this_thread::sleep_for(std::chrono::seconds(1));

                // Send the first frame
                lockFrameToSend = true;

                INFO_LOG_FMT(FALCONNECT, "Reading data");
                const std::vector<u8> dataToSend = frameToSend->GetSocketData();
                INFO_LOG_FMT(FALCONNECT, "Data read");

                lockFrameToSend = false;

                char data[256];
                data[0] = static_cast<char>(PacketType::DATA_FULL);

                std::memcpy(data + 1, dataToSend.data(), dataToSend.size());

                send(remoteSocket, data, sizeof(data), 0);

                std::this_thread::sleep_for(std::chrono::milliseconds(25));

                break;
            }

            case PacketType::DATA_FULL: {
                INFO_LOG_FMT(FALCONNECT, "DATA_FULL");
                // Set last read frame
                if (timeBeforePing != 0) ping = time(nullptr) - timeBeforePing;
                INFO_LOG_FMT(FALCONNECT, "Receiving...");
                const std::vector<u8> vect(buffer + 1, buffer + sizeof(buffer));

                RacerMemoryBlock* block = RacerMemoryBlock::CreateFromSocketData(vect);
                operationQueue.push(OperationType::SET_RACER_BLOCK);
                operationArgumentsQueue.emplace(*block);

                // If exited, stop sending frames
                if (exited) {
                    // Reset everything, instruct partner to do the same
                    hasGridded = false;
                    start = false;
                    exited = false;
                    canLoad = false;

                    char data[256];
                    data[0] = static_cast<char>(PacketType::RESET);

                    send(remoteSocket, data, sizeof(data), 0);

                    // If we're the host, wait until we can load
                    if (isHost) {
                        // ReSharper disable once CppDFAEndlessLoop
                        while (!canLoad) {
                            INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                        INFO_LOG_FMT(FALCONNECT, "Waiting for partner...");
                    } else {
                        // Wait until we can load, then tell the host we're ready
                        while (!canLoad) {
                            INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }

                        char data[256];
                        data[0] = static_cast<char>(PacketType::READY_TO_START);

                        send(remoteSocket, data, sizeof(data), 0);
                    }

                } else {
                    INFO_LOG_FMT(FALCONNECT, "Sending...");

                    // Send frame to be sent to remote
                    timeBeforePing = time(nullptr);

                    lockFrameToSend = true;

                    const std::vector<u8> dataToSend = frameToSend->GetSocketData();

                    lockFrameToSend = false;

                    char data[256];
                    data[0] = static_cast<char>(PacketType::DATA_FULL);

                    std::memcpy(data + 1, dataToSend.data(), dataToSend.size());

                    send(remoteSocket, data, sizeof(data), 0);

                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }

                break;
            }

            case PacketType::RESET: {
                hasGridded = false;
                start = false;
                exited = false;
                canLoad = false;

                // If we're the host, wait until we can load
                if (isHost) {
                    // ReSharper disable once CppDFAEndlessLoop
                    while (!canLoad) {
                        INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    INFO_LOG_FMT(FALCONNECT, "Waiting for partner...");
                } else {
                    // Wait until we can load, then tell the host we're ready
                    while (!canLoad) {
                        INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }

                    char data[256];
                    data[0] = static_cast<char>(PacketType::READY_TO_START);

                    send(remoteSocket, data, sizeof(data), 0);
                }

                break;
            }

            case PacketType::READY_TO_START: {
                char data[256];
                data[0] = static_cast<char>(PacketType::RACER_ID);
                data[1] = FalconnectManager::instance->racerIDs[0];

                send(remoteSocket, data, sizeof(data), 0);
            }

            default:
                break;
        }
    }
}
