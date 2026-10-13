#ifndef DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#define DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <variant>
#include <netinet/in.h>

#include "RacerMemoryBlock.h"
#include "Core/Falconnect/CourseSettings.h"
#include "Common/CommonTypes.h"


class FalconnectSocketManager {
public:
    static FalconnectSocketManager* instance;

    static constexpr std::size_t RACE_PACKET_HEADER_SIZE = 5;
    static constexpr std::size_t MAX_RACERS = 30;
    static constexpr std::size_t SERVER_RACER_RECORD_SIZE =
        RacerMemoryBlock::SOCKET_DATA_SIZE + sizeof(u32);
    static constexpr std::size_t FULL_RACE_PACKET_SIZE =
        RACE_PACKET_HEADER_SIZE + (MAX_RACERS * SERVER_RACER_RECORD_SIZE);
    static constexpr std::size_t CLIENT_FULL_RACE_PACKET_SIZE =
        RACE_PACKET_HEADER_SIZE + (MAX_RACERS * RacerMemoryBlock::SOCKET_DATA_SIZE);

    static constexpr std::size_t ClientRacePacketSize(const std::size_t racer_count)
    {
      return RACE_PACKET_HEADER_SIZE + (racer_count * RacerMemoryBlock::SOCKET_DATA_SIZE);
    }

    struct ReceivedRacerData
    {
        RacerMemoryBlock block{};
        u8 racer_index = 0;
        u32 source_sequence = 0;
        bool valid = false;
        bool position_updated = false;
    };

    struct ReceivedRaceData
    {
        std::array<ReceivedRacerData, MAX_RACERS> racers{};
    };

    std::thread socketThread[1];

    bool lockFrameToSend = false;

    void SocketThread();
    void DataThread(u32 race_generation);
    void SendDataThread(u32 race_generation);
    void SendUdpPingIfDue();
    void SendFrame(RacerMemoryBlock *frame, u8 index);
    std::vector<u8> GetCpuRacerIndices() const;
    void SetCpuRacerIndices(std::vector<u8> indices);
    void SetSelectedCustomMachineData(std::vector<u8> machine_data);
    bool UsesSelectedCustomMachine() const;
    void SetRemoteCustomMachineData(std::vector<u8> machine_data);
    std::vector<u8> GetCustomMachineDataForRace() const;
    void HandleConnectionLost();
    bool OwnsCpuRacer(u8 racer) const;
    bool ConsumeReceivedRaceData(ReceivedRaceData* received_data);
    Falconnect::CourseSettings GetCourseSettings() const;

    bool hasConnected = false;
    std::string connectionError;
    bool hasProperlyConnected = false;
    bool isError = false;
    bool shouldRun = true;
    bool isHost = false;
    bool hasGridded = false;
    bool start = false;
    bool exited = false;
    bool canLoad = false;
    bool shouldDisconnect = false;
    bool shouldSendData = false;
    bool hasReceivedAnyDataEver = false;

    std::atomic<u16> ping{0};
    std::atomic<bool> hasPingSample{false};
    std::atomic<u16> packetRate{0};
    u8 playerNumber = 0;
    u8 racerId = 6;
    u8 selectedCourse = 1;
    u8 ourCpus = 0;
    u8 readyPlayerCount = 0;
    u8 totalPlayerCount = 0;
    u8 cpuStartIndex = 1;
    std::vector<u8> name;
    std::vector<std::vector<u8>> names;
    u32 lastPacketNum = 0;

    //std::queue<OperationType> operationQueue;
    //std::queue<std::variant<u8, RacerMemoryBlock, std::string>> operationArgumentsQueue;

    RacerMemoryBlock* ourLastKnownData = nullptr;

private:
    void Start();
    void NotifyServerWhenGridded() const;
    void NotifyServerWhenReadyToLoad() const;

    RacerMemoryBlock* framesToSend[30] = {};

    int localSocket = 0;
    int serverSocket = 0;
    int serverUdpSocket = -1;
    sockaddr_in serverUdpAddress{};
    bool hasStarted = false;
    bool doneFirst = false;

    mutable std::mutex m_course_settings_mutex;
    Falconnect::CourseSettings m_course_settings;

    std::atomic<bool> shouldRunDataThread{true};
    // Detached UDP workers belong to one race only. A shared boolean is insufficient because a
    // previous worker can observe it becoming true for the next race and resume its old packet
    // sequence.
    std::atomic<u32> m_race_data_generation{0};

    std::mutex m_ping_mutex;
    std::chrono::steady_clock::time_point m_last_ping_sent_at{};
    std::chrono::steady_clock::time_point m_pending_ping_sent_at{};
    u32 m_next_ping_sequence = 0;
    u32 m_pending_ping_sequence = 0;
    bool m_has_pending_ping = false;
    mutable std::mutex m_cpu_assignment_mutex;
    std::vector<u8> m_cpu_racer_indices;
    mutable std::mutex m_received_race_data_mutex;
    ReceivedRaceData m_received_race_data;
    bool m_received_race_data_ready = false;
    mutable std::mutex m_custom_machine_data_mutex;
    std::vector<u8> m_selected_custom_machine_data;
    std::vector<u8> m_remote_custom_machine_data;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
