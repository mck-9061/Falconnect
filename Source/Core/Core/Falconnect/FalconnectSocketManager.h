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

    bool hasConnected = false;
    bool hasProperlyConnected = false;
    bool isError = false;
    bool shouldRun = true;
    bool isHost = false;
    bool hasGridded = false;
    bool start = false;
    bool exited = false;
    bool canLoad = false;
    bool shouldDisconnect = false;

    u16 ping = 0;
    u8 playerNumber = 0;
    u8 racerId = 6;
    u8 selectedCourse = 1;
    u8 usedCourseId = 1;
    u8 cpuCount = 29;
    std::vector<u8> name;
    std::vector<std::vector<u8>> names;

    std::queue<OperationType> operationQueue;
    std::queue<std::variant<u8, RacerMemoryBlock, std::string>> operationArgumentsQueue;

private:
    void Start();

    const RacerMemoryBlock* frameToSend = nullptr;

    int localSocket = 0;
    int serverSocket = 0;
    bool hasStarted = false;
    bool doneFirst = false;

    u32 timeBeforePing = 0;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
