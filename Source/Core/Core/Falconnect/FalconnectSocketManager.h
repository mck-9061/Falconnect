#ifndef DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#define DOLPHIN_EMU_SERVERSOCKETMANAGER_H
#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <variant>
#include <netinet/in.h>

#include "RacerMemoryBlock.h"
#include "Core/Falconnect/RaceDataProtocol.h"
#include "Core/Falconnect/CourseSettings.h"
#include "Common/CommonTypes.h"


class FalconnectSocketManager {
public:
    static FalconnectSocketManager* instance;

    // Fixed TCP control frame sizing; race UDP uses RaceDataProtocol.h.
    static constexpr std::size_t RACE_PACKET_HEADER_SIZE = 5;
    static constexpr std::size_t MAX_RACERS = 30;
    static constexpr std::size_t SERVER_RACER_RECORD_SIZE =
        RacerMemoryBlock::SOCKET_DATA_SIZE + sizeof(u32);
    static constexpr std::size_t FULL_RACE_PACKET_SIZE =
        RACE_PACKET_HEADER_SIZE + (MAX_RACERS * SERVER_RACER_RECORD_SIZE);
    using ReceivedRacerData = Falconnect::ReceivedRacerData;

    struct ReceivedRaceData
    {
        std::array<ReceivedRacerData, MAX_RACERS> racers{};
    };

    std::thread socketThread[1];


    void SocketThread();
    void DataThread();
    void SendDataThread();
    using OutgoingRaceData = std::array<std::optional<RacerMemoryBlock>, MAX_RACERS>;
    // Publish one complete frame sampled by FalconnectManager::Update on the CPU thread.
    void PublishFrame(OutgoingRaceData frame, const std::vector<u8>& cpu_racer_indices);
    std::vector<u8> GetCpuRacerIndices() const;
    void SetCpuRacerIndices(std::vector<u8> indices);
    void SetSelectedCustomMachineData(std::vector<u8> machine_data);
    bool UsesSelectedCustomMachine() const;
    void SetRemoteCustomMachineData(std::vector<u8> machine_data);
    std::vector<u8> GetCustomMachineDataForRace() const;
    void HandleConnectionLost();
    bool OwnsCpuRacer(u8 racer) const;
    void ConsumeReceivedRaceData(ReceivedRaceData* received_data);
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
    bool hasReceivedAnyDataEver = false;

    u16 ping = 0;
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


private:
    void Start();
    void NotifyServerWhenGridded();
    void NotifyServerWhenReadyToLoad();

    std::mutex m_outgoing_mutex;
    OutgoingRaceData m_frames_to_send{};
    bool m_force_full_update = true;

    int localSocket = 0;
    int serverSocket = 0;
    int serverUdpSocket = -1;
    sockaddr_in serverUdpAddress{};
    bool hasStarted = false;


    mutable std::mutex m_course_settings_mutex;
    Falconnect::CourseSettings m_course_settings;

    std::atomic<bool> shouldRunDataThread{true};

    u32 timeBeforePing = 0;
    mutable std::mutex m_cpu_assignment_mutex;
    std::vector<u8> m_cpu_racer_indices;
    mutable std::mutex m_received_race_data_mutex;
    ReceivedRaceData m_received_race_data;
    mutable std::mutex m_custom_machine_data_mutex;
    std::vector<u8> m_selected_custom_machine_data;
    std::vector<u8> m_remote_custom_machine_data;
};



#endif //DOLPHIN_EMU_SERVERSOCKETMANAGER_H
