#ifndef DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#define DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <variant>
#include <netinet/in.h>

#include "RacerMemoryBlock.h"
#include "Common/CommonTypes.h"


class FalconnectSocketManager {
public:
    static FalconnectSocketManager* instance;

    static constexpr std::size_t RACE_PACKET_HEADER_SIZE = 5;
    static constexpr std::size_t MAX_RACERS = 30;
    static constexpr std::size_t FULL_RACE_PACKET_SIZE =
        RACE_PACKET_HEADER_SIZE + (MAX_RACERS * RacerMemoryBlock::SOCKET_DATA_SIZE);

    static constexpr std::size_t RacePacketSize(const std::size_t racer_count)
    {
      return RACE_PACKET_HEADER_SIZE + (racer_count * RacerMemoryBlock::SOCKET_DATA_SIZE);
    }

    std::thread socketThread[1];

    bool lockFrameToSend = false;

    void SocketThread();
    void DataThread();
    void SendDataThread();
    void SendFrame(RacerMemoryBlock *frame, u8 index);
    std::vector<u8> GetCpuRacerIndices() const;
    void SetCpuRacerIndices(std::vector<u8> indices);
    void SetSelectedCustomMachineData(std::vector<u8> machine_data);
    bool UsesSelectedCustomMachine() const;
    void SetRemoteCustomMachineData(std::vector<u8> machine_data);
    std::vector<u8> GetCustomMachineDataForRace() const;
    void HandleConnectionLost();

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
    bool hasReceived = false;
    bool shouldSendData = false;
    bool hasReceivedAnyDataEver = false;

    u16 ping = 0;
    u8 playerNumber = 0;
    u8 racerId = 6;
    u8 selectedCourse = 1;
    u8 usedCourseId = 1;
    u8 cpuCount = 29;
    u8 ourCpus = 0;
    u8 readyPlayerCount = 0;
    u8 totalPlayerCount = 0;
    u8 cpuStartIndex = 1;
    std::vector<u8> name;
    std::vector<std::vector<u8>> names;
    u32 lastPacketNum = 0;

    //std::queue<OperationType> operationQueue;
    //std::queue<std::variant<u8, RacerMemoryBlock, std::string>> operationArgumentsQueue;

    RacerMemoryBlock* allBlocks[30] = {};
    RacerMemoryBlock* ourLastKnownData = nullptr;
    u8 usedIndices[30] = {};
    bool updated[30] = {};

private:
    void Start();
    void NotifyServerWhenGridded();
    void NotifyServerWhenReadyToLoad();

    RacerMemoryBlock* framesToSend[30] = {};

    int localSocket = 0;
    int serverSocket = 0;
    int serverUdpSocket = -1;
    sockaddr_in serverUdpAddress{};
    bool hasStarted = false;
    bool doneFirst = false;

    bool shouldRunDataThread = true;

    u32 timeBeforePing = 0;

    bool OwnsCpuRacer(u8 racer) const;
    mutable std::mutex m_cpu_assignment_mutex;
    std::vector<u8> m_cpu_racer_indices;
    mutable std::mutex m_custom_machine_data_mutex;
    std::vector<u8> m_selected_custom_machine_data;
    std::vector<u8> m_remote_custom_machine_data;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
