#include "ServerSocketManager.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include "expr.h"
#include "PacketType.h"
#include "Common/Logging/Log.h"

void ServerSocketManager::Start() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    const int a = bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress),
         sizeof(serverAddress));
    INFO_LOG_FMT(FALCONNECT, "Bind: {}", a);

    listen(serverSocket, 5);

    // Accept client
    clientSocket = accept(serverSocket, nullptr, nullptr);

    hasStarted = true;
}

void ServerSocketManager::SendFrame(const RacerMemoryBlock* frame) {
    frameToSend = frame;
}

void ServerSocketManager::ServerThread() {
    while (shouldRun) {
        if (!hasStarted) continue;

        // Read data from client
        char buffer[256] = { 0 };
        recv(clientSocket, buffer, sizeof(buffer), 0);

        switch (static_cast<PacketType>(buffer[0])) {
            case PacketType::DATA_FULL: {
                // Set last read frame
                const std::vector<u8> vect(buffer + 1, buffer + sizeof(buffer));

                RacerMemoryBlock* block = RacerMemoryBlock::CreateFromSocketData(vect);
                operationQueue.push(OperationType::SET_RACER_BLOCK);
                operationArgumentsQueue.emplace(*block);

                // Increment total exchanged frames
                totalExchangedFrames++;

                // Send frame to be sent to client
                const std::vector<u8> dataToSend = frameToSend->GetSocketData();
                char data[256];
                data[0] = static_cast<char>(PacketType::DATA_FULL);

                std::memcpy(data + 1, dataToSend.data(), dataToSend.size());

                send(clientSocket, data, sizeof(data), 0);

                break;
            }

            default:
                break;
        }
    }
}
