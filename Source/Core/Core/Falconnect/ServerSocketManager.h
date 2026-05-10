#ifndef DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#define DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#include <queue>
#include <variant>

#include "OperationType.h"
#include "RacerMemoryBlock.h"
#include "Common/CommonTypes.h"


class ServerSocketManager {
public:
    void Start();
    void SendFrame(const RacerMemoryBlock* frame);

    void ServerThread();

    bool shouldRun = true;

    std::queue<OperationType> operationQueue;
    std::queue<std::variant<u8, RacerMemoryBlock, std::string>> operationArgumentsQueue;

private:
    const RacerMemoryBlock* frameToSend = nullptr;

    int serverSocket = 0;
    int clientSocket = 0;
    bool hasStarted = false;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
