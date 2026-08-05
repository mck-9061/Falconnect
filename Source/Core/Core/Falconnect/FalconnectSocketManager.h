#ifndef DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#define DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#include <queue>
#include <thread>
#include <variant>
#include <netinet/in.h>

#include "OperationType.h"
#include "RacerMemoryBlock.h"
#include "Common/CommonTypes.h"


class FalconnectSocketManager {
public:
    static FalconnectSocketManager* instance;

    std::thread socketThread[1];

    bool lockFrameToSend = false;

    void SocketThread();
    void DataThread();
    void SendFrame(RacerMemoryBlock *frame, u8 index);

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
    u8 ourCpus = 0;
    u8 cpuStartIndex = 1;
    std::vector<u8> name;
    std::vector<std::vector<u8>> names;
    u32 lastPacketNum = 0;

    //std::queue<OperationType> operationQueue;
    //std::queue<std::variant<u8, RacerMemoryBlock, std::string>> operationArgumentsQueue;

    RacerMemoryBlock* allBlocks[30];
    u8 usedIndices[30];
    bool updated[30];

private:
    void Start();

    RacerMemoryBlock* framesToSend[30] = {};

    int localSocket = 0;
    int serverSocket = 0;
    int serverUdpSocket = 0;
    sockaddr_in serverUdpAddress{};
    bool hasStarted = false;
    bool doneFirst = false;

    bool shouldRunDataThread = true;

    u32 timeBeforePing = 0;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
