#ifndef DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#define DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#include <queue>
#include <thread>
#include <variant>

#include "OperationType.h"
#include "RacerMemoryBlock.h"
#include "Common/CommonTypes.h"


class FalconnectSocketManager {
public:
    static FalconnectSocketManager* instance;

    std::thread socketThread[1];

    bool lockFrameToSend = false;

    void SocketThread();
    void SendFrame(const RacerMemoryBlock* frame);

    bool shouldRun = true;
    bool isHost = false;
    bool hasGridded = false;
    bool start = false;

    u8 ping = 0;

    std::queue<OperationType> operationQueue;
    std::queue<std::variant<u8, RacerMemoryBlock, std::string>> operationArgumentsQueue;

private:
    void Start();

    const RacerMemoryBlock* frameToSend = nullptr;

    int localSocket = 0;
    int remoteSocket = 0;
    bool hasStarted = false;
    bool doneFirst = false;

    u32 timeBeforePing = 0;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
