#include "FalconnectSocketManager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include "ClientState.h"
#include "Common/Assert.h"
#include "SFML/System/String.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <wS2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "expr.h"
#endif

#include <chrono>
#include <limits>
#include <thread>

#include "FalconnectManager.h"
#include "PacketType.h"
#include "Core/Falconnect/ProtocolVersion.h"
#include "Common/Logging/Log.h"

FalconnectSocketManager* FalconnectSocketManager::instance = nullptr;

void FalconnectSocketManager::Start() {
    // Connect to the server
    INFO_LOG_FMT(FALCONNECT, "Connecting...");
    int code = -1;

    for (u8 attempt = 1; attempt <= 10; attempt++) {
        INFO_LOG_FMT(FALCONNECT, "Attempt {}", attempt);
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(8000);

        inet_pton(AF_INET, "162.19.231.212", &serverAddress.sin_addr); // Remote IP address
        //inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr); // Remote IP address

        code = connect(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress));
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (code != -1) break;
    }
    INFO_LOG_FMT(FALCONNECT, "Code: {}", code);

    if (code < 0) {
        isError = true;
    } else {
        connectionError = Falconnect::ExchangeProtocolVersion(serverSocket);
        if (!connectionError.empty()) {
            ERROR_LOG_FMT(FALCONNECT, "{}", connectionError);
#ifdef _WIN32
            closesocket(serverSocket);
#else
            close(serverSocket);
#endif
            shouldRun = false;
            shouldDisconnect = true;
            isError = true;
            return;
        }
        hasConnected = true;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void FalconnectSocketManager::SendFrame(RacerMemoryBlock *frame, const u8 index) {
    framesToSend[index] = frame;
    doneFirst = true;
}

bool FalconnectSocketManager::OwnsCpuRacer(const u8 racer) const
{
    const auto cpu_racer_indices = GetCpuRacerIndices();
    return std::find(cpu_racer_indices.begin(), cpu_racer_indices.end(), racer) != cpu_racer_indices.end();
}

Falconnect::CourseSettings FalconnectSocketManager::GetCourseSettings() const
{
    const std::lock_guard<std::mutex> lock(m_course_settings_mutex);
    return m_course_settings;
}

bool FalconnectSocketManager::ConsumeReceivedRaceData(ReceivedRaceData* received_data)
{
    const std::lock_guard<std::mutex> lock(m_received_race_data_mutex);
    if (!m_received_race_data_ready)
        return false;

    *received_data = m_received_race_data;
    for (ReceivedRacerData& received_racer : m_received_race_data.racers)
        received_racer.position_updated = false;
    m_received_race_data_ready = false;
    return true;
}

std::vector<u8> FalconnectSocketManager::GetCpuRacerIndices() const
{
    const std::lock_guard<std::mutex> lock(m_cpu_assignment_mutex);
    return m_cpu_racer_indices;
}

void FalconnectSocketManager::SetCpuRacerIndices(std::vector<u8> indices)
{
    const std::lock_guard<std::mutex> lock(m_cpu_assignment_mutex);
    const std::vector<u8> previous_indices = m_cpu_racer_indices;

    // A newly owned CPU may still have a remote block cached from before the handoff. Leaving it
    // in this cache makes FalconnectManager write that stale remote state over the local AI every
    // frame, so the CPU appears to have an engine but cannot drive correctly.
    for (const u8 racer : indices)
    {
        if (std::find(previous_indices.begin(), previous_indices.end(), racer) !=
            previous_indices.end())
        {
            continue;
        }

        const std::lock_guard<std::mutex> received_data_lock(m_received_race_data_mutex);
        for (ReceivedRacerData& received_racer : m_received_race_data.racers)
        {
            if (received_racer.valid && received_racer.racer_index == racer)
            {
                received_racer.valid = false;
                received_racer.position_updated = false;
            }
        }
    }

    // CPU records are positional in an outgoing UDP packet. Do not send frames prepared for the
    // previous assignment under the new assignment's slot IDs; wait for the next game update to
    // populate a fresh set.
    for (std::size_t i = 1; i < std::size(framesToSend); i++)
        framesToSend[i] = nullptr;

    ourCpus = static_cast<u8>(indices.size());
    m_cpu_racer_indices = std::move(indices);
    // INFO_LOG_FMT(FALCONNECT, "CPU assignment updated: {} CPU racers", ourCpus);
    for (std::size_t i = 0; i < m_cpu_racer_indices.size(); i++)
        INFO_LOG_FMT(FALCONNECT, "TIMING ASSIGN player={} cpu_slot={} local_racer={} server_racer={}",
                     playerNumber, i + 1, m_cpu_racer_indices[i], m_cpu_racer_indices[i]);
}

void FalconnectSocketManager::SetSelectedCustomMachineData(std::vector<u8> machine_data)
{
    const std::lock_guard<std::mutex> lock(m_custom_machine_data_mutex);
    const bool has_custom_component =
        std::any_of(machine_data.begin(), machine_data.end(), [](const u8 component) {
            return component != 0;
        });
    if (!has_custom_component)
        machine_data.clear();
    m_selected_custom_machine_data = std::move(machine_data);
}

bool FalconnectSocketManager::UsesSelectedCustomMachine() const
{
    const std::lock_guard<std::mutex> lock(m_custom_machine_data_mutex);
    return !m_selected_custom_machine_data.empty();
}

void FalconnectSocketManager::SetRemoteCustomMachineData(std::vector<u8> machine_data)
{
    const std::lock_guard<std::mutex> lock(m_custom_machine_data_mutex);
    m_remote_custom_machine_data = std::move(machine_data);
}

std::vector<u8> FalconnectSocketManager::GetCustomMachineDataForRace() const
{
    const std::lock_guard<std::mutex> lock(m_custom_machine_data_mutex);
    // The server sends the complete compact list in this client's racer order, including our
    // selected machine when it is custom.  Returning only that canonical list prevents the
    // local three-byte selection from being written twice.
    return m_remote_custom_machine_data;
}

bool recvAll(const int sock, char* buffer, const int size)
{
    int total = 0;

    while (total < size)
    {
        const int n = recv(sock, buffer + total, size - total, 0);

        if (n <= 0)
            return false;

        total += n;
    }

    return true;
}

void FalconnectSocketManager::HandleConnectionLost()
{
    if (isError || shouldDisconnect)
        return;

    // INFO_LOG_FMT(FALCONNECT, "Lost connection to Falconnect server");
    shouldRunDataThread = false;
    shouldDisconnect = true;
    shouldRun = false;
    isError = true;

    if (FalconnectManager::instance != nullptr)
    {
        SuccessAlertFmt("You lost connection to the Falconnect server.");
        FalconnectManager::instance->shouldReset = true;
    }
}

void FalconnectSocketManager::RequestDisconnect()
{
    shouldRunDataThread = false;
    shouldDisconnect = true;
    shouldRun = false;

    // SocketThread normally blocks in recvAll waiting for TCP control packets. Interrupt that
    // blocking receive so its teardown can close the connection and the server sees EOF.
    if (serverSocket == -1)
        return;
#ifdef _WIN32
    shutdown(serverSocket, SD_BOTH);
#else
    shutdown(serverSocket, SHUT_RDWR);
#endif
}

void FalconnectSocketManager::NotifyServerWhenGridded() const {
    std::this_thread::sleep_for(std::chrono::seconds(5));

    while (!hasGridded && !shouldDisconnect)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    if (shouldDisconnect || !shouldRun)
        return;

    char data[FULL_RACE_PACKET_SIZE] = { 0 };
    data[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
    data[1] = static_cast<char>(ClientState::GRIDDED);
    data[2] = 1;

    send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);
}

void FalconnectSocketManager::NotifyServerWhenReadyToLoad() const {
    while (!canLoad && !raceLoadRequested && !shouldDisconnect)
    {
        INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (shouldDisconnect || !shouldRun)
        return;

    if (raceLoadRequested) return;

    // First send our selected racer ID, then say we're ready.
    char data[FULL_RACE_PACKET_SIZE] = { 0 };
    data[0] = static_cast<char>(ToServerPacketType::SETTINGS);
    data[1] = static_cast<char>(racerId);
    data[2] = static_cast<char>(selectedCourse);
    {
        const std::lock_guard<std::mutex> lock(m_custom_machine_data_mutex);
        for (std::size_t i = 0; i < m_selected_custom_machine_data.size() && i < 3; i++)
            data[i + 3] = static_cast<char>(m_selected_custom_machine_data[i]);
    }
    send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);

    char data2[FULL_RACE_PACKET_SIZE] = { 0 };
    data2[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
    data2[1] = static_cast<char>(ClientState::READY);
    data2[2] = 1;
    send(serverSocket, data2, FULL_RACE_PACKET_SIZE, 0);
}

void FalconnectSocketManager::SocketThread() {
    Start();
    // The manager still needs this object's connection result to report a failed handshake.
    if (isError)
        return;

    std::this_thread::sleep_for(std::chrono::seconds(1));

    int invalidPacketCount = 0;

    while (shouldRun && !isError) {
        if (shouldDisconnect) {
            break;
        }

        // Read data from server
        char buffer[FULL_RACE_PACKET_SIZE] = { 0 };
        if (!recvAll(serverSocket, buffer, sizeof(buffer))) {
            HandleConnectionLost();
            break;
        }

        switch (static_cast<FromServerPacketType>(buffer[0])) {
            case FromServerPacketType::DISCONNECT: {
                // INFO_LOG_FMT(FALCONNECT, "Disconnecting!");
                hasProperlyConnected = true;
                auto reason = std::string(buffer + 1);
                SuccessAlertFmt("You have been disconnected from the Falconnect server.\n"
                                "Reason: {}", reason);

                FalconnectManager::instance->shouldReset = true;
                shouldRun = false;
                shouldDisconnect = true;
                isError = true;
                break;
            }

            case FromServerPacketType::CONNECTED: {
              if (buffer[1] == 0)
              {
                // INFO_LOG_FMT(FALCONNECT, "Dropping invalid packet!");
                  invalidPacketCount++;

                  if (invalidPacketCount > 50) {
                      // Server probably closed, or we lost connection
                      // INFO_LOG_FMT(FALCONNECT, "Disconnecting!");
                      {
                          SuccessAlertFmt("You lost connection to the Falconnect server.");
                      }
                      std::this_thread::sleep_for(std::chrono::seconds(1));
                      shouldDisconnect = true;
                      shouldRun = false;
                      isError = true;
                      FalconnectManager::instance->shouldReset = true;
                  }
                break;
              }

                hasProperlyConnected = true;
                playerNumber = buffer[1];
                ourCpus = buffer[2];
                cpuStartIndex = buffer[3];
                const u16 serverUdpPort =
                    (static_cast<u16>(static_cast<u8>(buffer[4])) << 8) |
                    static_cast<u8>(buffer[5]);

                // Setup UDP
#ifdef _WIN32
                if (serverUdpSocket != -1) closesocket(serverUdpSocket);
#else
                if (serverUdpSocket != -1) close(serverUdpSocket);
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                serverUdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

                serverUdpAddress.sin_family = AF_INET;
                serverUdpAddress.sin_port = htons(serverUdpPort);

                //inet_pton(AF_INET, "127.0.0.1", &serverUdpAddress.sin_addr); // Remote IP address
                inet_pton(AF_INET, "162.19.231.212", &serverUdpAddress.sin_addr); // Remote IP address

                // Send our name
                char data1[FULL_RACE_PACKET_SIZE] = { 0 };
                data1[0] = static_cast<char>(ToServerPacketType::NAME);

                for (std::size_t i = 0; i < 32 && i < name.size(); i++) {
                    data1[i + 1] = static_cast<char>(name[i]);
                }

                send(serverSocket, data1, FULL_RACE_PACKET_SIZE, 0);

                // Do not block this TCP reader on the local menu. Lobby-status updates must
                // continue to arrive until the player chooses their settings.
                std::thread readyThread(&FalconnectSocketManager::NotifyServerWhenReadyToLoad, this);
                readyThread.detach();

                break;
            }

            default:
                invalidPacketCount = 0;
                break;

            case FromServerPacketType::COURSE: {
                const auto settings = Falconnect::CourseSettings::Decode(
                    {reinterpret_cast<const u8*>(buffer), sizeof(buffer)});
                if (!settings) {
                    ERROR_LOG_FMT(FALCONNECT, "Server sent invalid course settings");
                    HandleConnectionLost();
                    break;
                }
                {
                    const std::lock_guard<std::mutex> lock(m_course_settings_mutex);
                    m_course_settings = *settings;
                }
                INFO_LOG_FMT(FALCONNECT, "Track {} ({}): {} laps, boost on lap {}",
                             settings->track_id, settings->track_name, settings->lap_count,
                             settings->boost_lap);

                break;
            }

            case FromServerPacketType::RACER_IDS: {
                // Construct array
                for (u8 i = 0; i < 30; i++) {
                    u8 usedIndex = i;
                    if (i == 0) usedIndex = playerNumber - 1;
                    if (i == playerNumber - 1) {
                        // INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);
                        raceRacerId = static_cast<u8>(buffer[i + 1]);
                        continue; // Skip our data
                    }

                    const char racerNum = buffer[i + 1];
                    FalconnectManager::instance->racerIDs[usedIndex - 1] = racerNum;
                }

                break;
            }

            case FromServerPacketType::NAMES: {
                names.clear();

                for (int i = 0; i < 30; i++) {
                    const std::vector<u8> iHateCpp(buffer + 1 + (i * 32), buffer + 1 + ((i + 1) * 32));

                    std::string str(iHateCpp.begin(), iHateCpp.end());
                    INFO_LOG_FMT(FALCONNECT, "Received name: {}", str);

                    names.push_back(iHateCpp);
                }

                break;
            }

            case FromServerPacketType::CPU_ASSIGNMENT: {
                std::vector<u8> cpu_racer_indices;
                cpu_racer_indices.reserve(static_cast<u8>(buffer[1]));
                for (u8 i = 0; i < static_cast<u8>(buffer[1]); i++)
                    cpu_racer_indices.push_back(static_cast<u8>(buffer[i + 2]));
                SetCpuRacerIndices(std::move(cpu_racer_indices));
                break;
            }

            case FromServerPacketType::LOBBY_STATUS: {
                readyPlayerCount = static_cast<u8>(buffer[1]);
                totalPlayerCount = static_cast<u8>(buffer[2]);
                forceRaceStartCountdownSeconds =
                    (static_cast<u16>(static_cast<u8>(buffer[3])) << 8) |
                    static_cast<u16>(static_cast<u8>(buffer[4]));
                INFO_LOG_FMT(FALCONNECT, "Lobby status: {}/{} players ready, force start in {}s",
                             readyPlayerCount, totalPlayerCount, forceRaceStartCountdownSeconds);
                break;
            }

            case FromServerPacketType::CUSTOM_MACHINE_DATA: {
                constexpr std::size_t components_per_machine = 3;
                const std::size_t machine_count = static_cast<u8>(buffer[1]);
                const std::size_t data_size = machine_count * components_per_machine;
                if (data_size > FULL_RACE_PACKET_SIZE - 2) {
                    INFO_LOG_FMT(FALCONNECT, "Discarding invalid custom-machine data packet");
                    break;
                }

                SetRemoteCustomMachineData(
                    std::vector<u8>(buffer + 2, buffer + 2 + data_size));
                INFO_LOG_FMT(FALCONNECT, "Received {} custom machines", machine_count);
                break;
            }

            case FromServerPacketType::START: {
                INFO_LOG_FMT(FALCONNECT, "START");
                raceLoadRequested = true;
                exited = false;
                hasGridded = false;
                // START begins a new load. Do not let a stale START_RACE flag from the previous
                // race start this countdown before the server has received every GRIDDED update.
                start = false;

                { // force the compiler to not be a whiny baby
                  FalconnectManager::instance->shouldStart = true;
                }

                // Keep this TCP reader free while the game loads. The server may need to hand
                // us CPUs before every player reaches the grid.
                std::thread griddedThread(&FalconnectSocketManager::NotifyServerWhenGridded, this);
                griddedThread.detach();

                break;
            }

            case FromServerPacketType::START_RACE: {
                INFO_LOG_FMT(FALCONNECT, "START_RACE");
                start = true;

                // Retire any detached UDP workers from the previous race before creating the
                // new pair. Their packet counter is local to the worker, so allowing one to
                // resume here would send race-one sequence numbers during race two.
                const u32 race_generation = m_race_data_generation.fetch_add(1) + 1;

                lastPacketNum = 0;
                hasReceivedAnyDataEver = false;
                m_has_sent_race_data_for_generation = false;
                ping.store(0);
                hasPingSample.store(false);
                packetRate.store(0);
                {
                    const std::lock_guard<std::mutex> lock(m_ping_mutex);
                    m_last_ping_sent_at = {};
                    m_pending_ping_sent_at = {};
                    m_pending_ping_sequence = 0;
                    m_has_pending_ping = false;
                }
                {
                    const std::lock_guard<std::mutex> lock(m_received_race_data_mutex);
                    m_received_race_data = {};
                    m_received_race_data_ready = false;
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));

                shouldRunDataThread = true;
                std::thread readThread(&FalconnectSocketManager::DataThread, this, race_generation);
                readThread.detach();

                std::thread sendThread(&FalconnectSocketManager::SendDataThread, this, race_generation);
                sendThread.detach();

                break;
            }
        }
    }

    // RequestDisconnect has already shut down the TCP socket to wake recvAll. Do not attempt a
    // final write on that half-closed descriptor (and risk SIGPIPE); the server observes EOF.
    if (!isError && !shouldDisconnect) {
        char data[FULL_RACE_PACKET_SIZE] = { 0 };
        data[0] = static_cast<char>(ToServerPacketType::DISCONNECT);

        send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    delete this;
}

void FalconnectSocketManager::SendUdpPingIfDue()
{
    constexpr auto ping_interval = std::chrono::seconds(2);
    const auto now = std::chrono::steady_clock::now();
    std::array<char, RACE_PACKET_HEADER_SIZE> data{};

    {
        const std::lock_guard<std::mutex> lock(m_ping_mutex);
        if (now - m_last_ping_sent_at < ping_interval)
            return;

        const u32 sequence = ++m_next_ping_sequence;
        data[0] = static_cast<char>(ToServerPacketType::UDP_PING);
        data[1] = static_cast<char>((sequence >> 24) & 0xff);
        data[2] = static_cast<char>((sequence >> 16) & 0xff);
        data[3] = static_cast<char>((sequence >> 8) & 0xff);
        data[4] = static_cast<char>(sequence & 0xff);
        m_last_ping_sent_at = now;
        m_pending_ping_sent_at = now;
        m_pending_ping_sequence = sequence;
        m_has_pending_ping = true;
    }

    sendto(serverUdpSocket, data.data(), static_cast<int>(data.size()), 0,
           reinterpret_cast<sockaddr*>(&serverUdpAddress), sizeof(serverUdpAddress));
}

void FalconnectSocketManager::SendDataThread(const u32 race_generation) {
    u32 sentCount = 0;
    auto last_timing_log = std::chrono::steady_clock::time_point{};

    while (shouldRunDataThread.load() && m_race_data_generation.load() == race_generation) {
        shouldSendData = false;

        if (FalconnectManager::instance->currentState != GameState::RACING) continue;
        SendUdpPingIfDue();
        // Send our frames
        sentCount++;

        while (framesToSend[0] == nullptr) {
            // INFO_LOG_FMT(FALCONNECT, "Bad frame!");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // A newer START_RACE can arrive while waiting for a frame. Do not let this retired
        // worker send even one packet from its old sequence into the new race.
        if (m_race_data_generation.load() != race_generation)
            break;

        //INFO_LOG_FMT(FALCONNECT, "Sending...");

        // Send frame to be sent to remote
        lockFrameToSend = true;

        char data[CLIENT_FULL_RACE_PACKET_SIZE] = { 0 };
        data[0] = static_cast<char>(FromServerPacketType::FULL_DATA);

        data[1] = static_cast<char>((sentCount >> 24) & 0xff);
        data[2] = static_cast<char>((sentCount >> 16) & 0xff);
        data[3] = static_cast<char>((sentCount >> 8) & 0xff);
        data[4] = static_cast<char>(sentCount & 0xff);

        const auto cpu_racer_indices = GetCpuRacerIndices();
        const std::size_t racer_count = cpu_racer_indices.size() + 1;
        const auto now = std::chrono::steady_clock::now();
        const bool log_timing = now - last_timing_log >= std::chrono::seconds(1);
        if (log_timing) last_timing_log = now;
        bool missing_frame = false;
        for (u8 frame_index = 0; frame_index < racer_count; frame_index++) {
            if (framesToSend[frame_index] == nullptr) {
                if (log_timing)
                    INFO_LOG_FMT(FALCONNECT,
                                 "TIMING TX player={} seq={} action=missing-frame slot={} racers={}",
                                 playerNumber, sentCount, frame_index, racer_count);
                missing_frame = true;
                break;
            }
        }
        if (missing_frame) {
            lockFrameToSend = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        int i = 0;
        int j = 1;
        for (const RacerMemoryBlock* frame : framesToSend) {
            if (j > racer_count) break;

            const std::vector<u8> dataToSend = frame->GetSocketData();

            std::memcpy(data + RACE_PACKET_HEADER_SIZE + i, dataToSend.data(), dataToSend.size());

            if (log_timing) {
                const u8 local_racer = j == 1 ? 0 : cpu_racer_indices[j - 2];
                const u8 server_racer = j == 1 ? playerNumber - 1 : local_racer;
                INFO_LOG_FMT(FALCONNECT,
                             "TIMING TX player={} seq={} slot={} kind={} local_racer={} "
                             "server_racer={} lap_data=[{:08x},{:08x},{:08x}] total_time_frames={}",
                             playerNumber, sentCount, j - 1, j == 1 ? "human" : "cpu", local_racer,
                             server_racer, frame->timingData[0], frame->timingData[1],
                             frame->timingData[2], frame->raceTimeFrameCount);
            }

            i += RacerMemoryBlock::SOCKET_DATA_SIZE;
            j++;
        }

        lockFrameToSend = false;

        const auto sent_bytes = sendto(serverUdpSocket,
            data,
            static_cast<int>(ClientRacePacketSize(racer_count)),
            0,
            reinterpret_cast<sockaddr *>(&serverUdpAddress),
            sizeof(serverUdpAddress));

        if (sent_bytes == static_cast<int>(ClientRacePacketSize(racer_count)) &&
            m_race_data_generation.load() == race_generation) {
            m_has_sent_race_data_for_generation = true;
        }

        if (log_timing)
            INFO_LOG_FMT(FALCONNECT, "TIMING TX RESULT player={} seq={} sent_bytes={} expected_bytes={}",
                         playerNumber, sentCount, sent_bytes, ClientRacePacketSize(racer_count));

        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
}

void FalconnectSocketManager::DataThread(const u32 race_generation) {
    const auto timeout_start = std::chrono::steady_clock::now();
    auto last_packet_time = timeout_start;
    auto packet_rate_window_started = timeout_start;
    u32 packets_in_rate_window = 0;
    auto last_timing_log = std::chrono::steady_clock::time_point{};

#ifdef _WIN32
    const DWORD receive_timeout = 1000;
    setsockopt(serverUdpSocket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&receive_timeout), sizeof(receive_timeout));
#else
    const timeval receive_timeout{1, 0};
    setsockopt(serverUdpSocket, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
#endif

    while (shouldRunDataThread.load() && m_race_data_generation.load() == race_generation) {
        // Receive datagram
        char buffer[FULL_RACE_PACKET_SIZE] = { 0 };
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);

        const int received = recvfrom(serverUdpSocket,
                                      buffer,
                                      sizeof(buffer),
                                      0,
                                      reinterpret_cast<sockaddr *>(&from),
                                      &fromLen);

        // recvfrom may have been blocking when a new race replaced this worker. Discard the
        // wake-up packet; the new generation owns the socket from this point on.
        if (m_race_data_generation.load() != race_generation)
            break;

        if (received == RACE_PACKET_HEADER_SIZE &&
            buffer[0] == static_cast<char>(FromServerPacketType::UDP_PONG)) {
            const u32 sequence =
                (static_cast<u32>(static_cast<u8>(buffer[1])) << 24) |
                (static_cast<u32>(static_cast<u8>(buffer[2])) << 16) |
                (static_cast<u32>(static_cast<u8>(buffer[3])) << 8) |
                static_cast<u32>(static_cast<u8>(buffer[4]));
            const auto pong_received_at = std::chrono::steady_clock::now();
            const std::lock_guard<std::mutex> lock(m_ping_mutex);
            if (m_has_pending_ping && sequence == m_pending_ping_sequence) {
                const auto round_trip_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                    pong_received_at - m_pending_ping_sent_at).count();
                ping.store(static_cast<u16>(std::min<long long>(
                    std::max<long long>(1, round_trip_milliseconds),
                    std::numeric_limits<u16>::max())));
                hasPingSample.store(true);
                m_has_pending_ping = false;
            }
            // A pong proves the UDP path is alive, but it is not race data. Preserve the
            // five-second full-snapshot fail-safe even if the server is only echoing pings.
            if (m_has_sent_race_data_for_generation.load() &&
                pong_received_at - last_packet_time >= std::chrono::seconds(5)) {
                HandleConnectionLost();
                break;
            }
            continue;
        }

        if (received != FULL_RACE_PACKET_SIZE ||
            buffer[0] != static_cast<char>(FromServerPacketType::FULL_DATA)) {
            // During a new race the server cannot address this UDP socket until we have sent a
            // packet from it. Starting the timeout earlier races the course/countdown setup and
            // can incorrectly disconnect a client at the beginning of race two.
            if (m_has_sent_race_data_for_generation.load() &&
                std::chrono::steady_clock::now() - last_packet_time >= std::chrono::seconds(5)) {
                HandleConnectionLost();
                break;
            }
            continue;
        }

        last_packet_time = std::chrono::steady_clock::now();

        ++packets_in_rate_window;
        const auto packet_received_at = std::chrono::steady_clock::now();
        if (packet_received_at - packet_rate_window_started >= std::chrono::seconds(1)) {
            const auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                packet_received_at - packet_rate_window_started).count();
            packetRate.store(static_cast<u16>(std::min<long long>(
                (packets_in_rate_window * 1000LL) / elapsed_milliseconds,
                std::numeric_limits<u16>::max())));
            packet_rate_window_started = packet_received_at;
            packets_in_rate_window = 0;
        }

        const u32 packetNum =
            ((buffer[1] & 0xff) << 24) |
                ((buffer[2] & 0xff) << 16) |
                    ((buffer[3] & 0xff) << 8) |
                        (buffer[4] & 0xff);

        if (packetNum <= lastPacketNum) {
            //INFO_LOG_FMT(FALCONNECT, "Skipping old packet {}", packetNum);
            continue;
        }

        lastPacketNum = packetNum;
        // INFO_LOG_FMT(FALCONNECT, "Packet {}", packetNum);

        if (const auto now = std::chrono::steady_clock::now(); now - last_timing_log >= std::chrono::seconds(1)) last_timing_log = now;

        const auto cpu_racer_indices = GetCpuRacerIndices();
        const std::lock_guard<std::mutex> received_data_lock(m_received_race_data_mutex);
        for (u8 i = 0; i < 30; i++) {
            u8 usedIndex = i;
            if (i == 0) usedIndex = playerNumber - 1;
            const std::size_t record_offset =
                RACE_PACKET_HEADER_SIZE + (i * SERVER_RACER_RECORD_SIZE);

            const u32 source_sequence =
                (static_cast<u32>(static_cast<u8>(buffer[record_offset + RacerMemoryBlock::SOCKET_DATA_SIZE])) << 24) |
                (static_cast<u32>(static_cast<u8>(buffer[record_offset + RacerMemoryBlock::SOCKET_DATA_SIZE + 1])) << 16) |
                (static_cast<u32>(static_cast<u8>(buffer[record_offset + RacerMemoryBlock::SOCKET_DATA_SIZE + 2])) << 8) |
                static_cast<u32>(static_cast<u8>(buffer[record_offset + RacerMemoryBlock::SOCKET_DATA_SIZE + 3]));

            if (i == playerNumber - 1) {
              // INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);

                // Store the server's last known position of us
                const std::vector<u8> racerData(
                    buffer + record_offset,
                    buffer + record_offset + RacerMemoryBlock::SOCKET_DATA_SIZE);
                ourLastKnownData = RacerMemoryBlock::CreateFromSocketData(racerData);

                continue; // Skip our data
            }

            if (std::ranges::find(cpu_racer_indices, usedIndex) !=
                cpu_racer_indices.end()) {
                // INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our CPU", i);
                m_received_race_data.racers[i].valid = false;
                continue;
            }

            // INFO_LOG_FMT(FALCONNECT, "Storing racer at index {} in slot {}", i, usedIndex);

            if (const std::vector<u8> racerData(
                    buffer + record_offset,
                    buffer + record_offset + RacerMemoryBlock::SOCKET_DATA_SIZE);
                std::ranges::all_of(racerData,
                                    [](const u8 value) { return value == 0; })) {
                // INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Invalid data", i);
                m_received_race_data.racers[i].valid = false;
            } else {
                const std::unique_ptr<RacerMemoryBlock> block(
                    RacerMemoryBlock::CreateFromSocketData(racerData));
                ReceivedRacerData& received_racer = m_received_race_data.racers[i];
                const u8 source_player = static_cast<u8>(source_sequence >> 24);
                const u32 source_packet_sequence = source_sequence & 0x00ffffff;
                if (received_racer.valid) {
                    const u8 previous_source_player =
                        static_cast<u8>(received_racer.source_sequence >> 24);
                    const u32 previous_source_packet_sequence =
                        received_racer.source_sequence & 0x00ffffff;
                    if (source_player == previous_source_player &&
                        source_packet_sequence <= previous_source_packet_sequence) {
                        continue;
                    }
                }

                const bool position_changed = !received_racer.valid ||
                    block->centerPosition[0] != received_racer.block.centerPosition[0] ||
                    block->centerPosition[1] != received_racer.block.centerPosition[1] ||
                    block->centerPosition[2] != received_racer.block.centerPosition[2];

                received_racer.block = *block;
                received_racer.racer_index = usedIndex;
                received_racer.source_sequence = source_sequence;
                received_racer.valid = true;
                received_racer.position_updated = received_racer.position_updated || position_changed;
            }
        }

        m_received_race_data_ready = true;
        hasReceivedAnyDataEver = true;

        // If exited, stop everything
        if (exited) {
            // Reset everything, tell server
            hasGridded = false;
            start = false;
            exited = false;
            canLoad = false;
            raceLoadRequested = false;
            // Do not stop a newer race if this is a delayed receiver from an older generation.
            if (m_race_data_generation.load() == race_generation)
                shouldRunDataThread = false;
            hasReceivedAnyDataEver = false;

            char data[FULL_RACE_PACKET_SIZE] = { 0 };
            data[0] = static_cast<char>(ToServerPacketType::RESET);

            send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);

            break;
        }

    }
}
