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
    inet_pton(AF_INET, "192.168.4.98", &remoteAddress.sin_addr); // Remote IP address

    INFO_LOG_FMT(FALCONNECT, "Connecting...");
    connect(remoteSocket, reinterpret_cast<struct sockaddr *>(&remoteAddress), sizeof(remoteAddress));
    INFO_LOG_FMT(FALCONNECT, "Connection established!");
}

void FalconnectSocketManager::SendFrame(const RacerMemoryBlock* frame) {
    frameToSend = frame;
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
                FalconnectManager::instance->shouldStart = true;
                break;
            }

            case PacketType::DATA_FULL: {
                // Set last read frame
                const std::vector<u8> vect(buffer + 1, buffer + sizeof(buffer));

                RacerMemoryBlock* block = RacerMemoryBlock::CreateFromSocketData(vect);
                operationQueue.push(OperationType::SET_RACER_BLOCK);
                operationArgumentsQueue.emplace(*block);

                // Send frame to be sent to remote
                const std::vector<u8> dataToSend = frameToSend->GetSocketData();
                char data[256];
                data[0] = static_cast<char>(PacketType::DATA_FULL);

                std::memcpy(data + 1, dataToSend.data(), dataToSend.size());

                send(remoteSocket, data, sizeof(data), 0);

                break;
            }

            default:
                break;
        }
    }
}
