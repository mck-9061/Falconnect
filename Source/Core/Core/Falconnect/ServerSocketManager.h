#ifndef DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#define DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#include "RacerMemoryBlock.h"
#include "Common/CommonTypes.h"


class ServerSocketManager {
public:
    void Start();
    void SendFrame(const RacerMemoryBlock& frame) const;

    void ServerThread();

    u16 totalExchangedFrames = 0;
    RacerMemoryBlock* lastReceivedFrame;

private:
    RacerMemoryBlock& frameToSend;

    int serverSocket = 0;
    int clientSocket = 0;
    bool hasStarted = false;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
