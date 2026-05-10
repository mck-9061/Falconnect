#include "ServerSocketManager.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include "expr.h"
#include "PacketType.h"

void ServerSocketManager::Start() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket
    bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress),
         sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    // accepting connection request
    clientSocket = accept(serverSocket, nullptr, nullptr);

    hasStarted = true;
}

void ServerSocketManager::SendFrame(const RacerMemoryBlock& frame) const {
    frameToSend = frame;
}

void ServerSocketManager::ServerThread() {
    // Read data from client
    char buffer[256] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);

    switch (auto type = static_cast<PacketType>(buffer[0])) {
        case PacketType::DATA_FULL: {
            // Set last read frame
            const std::vector<u8> vect(buffer + 1, buffer + sizeof(buffer));

            RacerMemoryBlock* block = RacerMemoryBlock::CreateFromSocketData(vect);
            lastReceivedFrame = block;

            // Increment total exchanged frames
            totalExchangedFrames++;

            // Send frame to be sent to client
            const std::vector<u8> dataToSend = frameToSend.GetSocketData();
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
