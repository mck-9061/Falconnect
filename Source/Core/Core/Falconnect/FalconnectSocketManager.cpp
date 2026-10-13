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

void FalconnectSocketManager::SendFrame(RacerMemoryBlock* frame, const u8 local_racer)
{
    const std::unique_ptr<RacerMemoryBlock> owned_frame(frame);
    if (!frame || local_racer >= MAX_RACERS)
        return;
    // Lock ownership before outgoing data, matching the sender and assignment update.
    const std::lock_guard<std::mutex> assignment_lock(m_cpu_assignment_mutex);
    if (local_racer != 0 && std::find(m_cpu_racer_indices.begin(), m_cpu_racer_indices.end(),
                                    local_racer) == m_cpu_racer_indices.end())
        return;
    const std::lock_guard<std::mutex> lock(m_outgoing_mutex);
    m_frames_to_send[local_racer] = *frame;
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

void FalconnectSocketManager::ConsumeReceivedRaceData(ReceivedRaceData* received_data)
{
    const std::lock_guard<std::mutex> lock(m_received_race_data_mutex);

    *received_data = m_received_race_data;
    for (ReceivedRacerData& received_racer : m_received_race_data.racers)
        received_racer.full_updated = false;

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
                received_racer.full_updated = false;
            }
        }
    }

    // Wait for fresh CPU-thread samples after ownership changes, then immediately send a full
    // update to establish the new owner's simulation state.
    {
        const std::lock_guard<std::mutex> outgoing_lock(m_outgoing_mutex);
        m_frames_to_send = {};
        m_force_full_update = true;
    }

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

void FalconnectSocketManager::NotifyServerWhenGridded()
{
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

void FalconnectSocketManager::NotifyServerWhenReadyToLoad()
{
    while (!canLoad && !shouldDisconnect)
    {
        INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (shouldDisconnect || !shouldRun)
        return;

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

        //std::this_thread::sleep_for(std::chrono::milliseconds(4));
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
                INFO_LOG_FMT(FALCONNECT, "Lobby status: {}/{} players ready", readyPlayerCount,
                             totalPlayerCount);
                FalconnectManager::instance->patcher->SetPlayersReadyText(instance->readyPlayerCount, instance->totalPlayerCount);
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
                exited = false;
                hasGridded = false;

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

                lastPacketNum = 0;
                hasReceivedAnyDataEver = false;
                timeBeforePing = 0;
                {
                    const std::lock_guard<std::mutex> lock(m_received_race_data_mutex);
                    m_received_race_data = {};
                }

                start = true;
                shouldRunDataThread = true;
                std::thread readThread(&FalconnectSocketManager::DataThread, this);
                readThread.detach();

                std::thread sendThread(&FalconnectSocketManager::SendDataThread, this);
                sendThread.detach();

                break;
            }
        }
    }

    if (!isError) {
        char data[FULL_RACE_PACKET_SIZE] = { 0 };
        data[0] = static_cast<char>(ToServerPacketType::DISCONNECT);

        send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    delete this;
}

void FalconnectSocketManager::SendDataThread()
{
    using SendClock = std::chrono::steady_clock;
    // 360 sends/second gives six transmission opportunities per normal 60 Hz GX frame.
    // Game memory is sampled only on the CPU thread at frame end.
    constexpr auto interval = std::chrono::nanoseconds(1'000'000'000 / 360);
    auto deadline = SendClock::now();
    auto next_full = deadline;
    u32 sequence = 0;
    while (shouldRunDataThread)
    {
        std::this_thread::sleep_until(deadline);
        const auto now = SendClock::now();
        deadline += interval;
        if (deadline <= now)
            deadline = now + interval;

        std::vector<u8> data{1};
        Falconnect::AppendRaceWord(data, ++sequence);
        bool full = false;
        {
            const std::lock_guard<std::mutex> assignment_lock(m_cpu_assignment_mutex);
            const std::lock_guard<std::mutex> outgoing_lock(m_outgoing_mutex);
            if (!m_frames_to_send[0])
                continue;
            bool ready = true;
            for (const u8 racer : m_cpu_racer_indices)
                ready &= m_frames_to_send[racer].has_value();
            if (!ready)
                continue;
            full = m_force_full_update || now >= next_full;
            const auto append = [&](u8 local_racer, u8 server_racer) {
                data.push_back(server_racer);
                data.push_back(full ? 1 : 0);
                const auto& block = *m_frames_to_send[local_racer];
                const auto payload = full ? block.GetSocketData() : block.GetInputSocketData();
                data.insert(data.end(), payload.begin(), payload.end());
                if (full)
                    INFO_LOG_FMT(FALCONNECT, "TIMING TX player={} seq={} local_racer={} server_racer={} words=[{:08x},{:08x},{:08x}]",
                                 playerNumber, sequence, local_racer, server_racer,
                                 block.timingData[0], block.timingData[1], block.timingData[2]);
            };
            append(0, playerNumber - 1);
            for (const u8 racer : m_cpu_racer_indices)
                append(racer, racer);
            m_force_full_update = false;
        }
        const auto sent = sendto(serverUdpSocket, reinterpret_cast<const char*>(data.data()), data.size(), 0,
                                reinterpret_cast<sockaddr*>(&serverUdpAddress), sizeof(serverUdpAddress));
        if (full && sent == static_cast<decltype(sent)>(data.size()))
            next_full = now + std::chrono::seconds(1);
    }
}

void FalconnectSocketManager::DataThread() {
    const auto timeout_start = std::chrono::steady_clock::now();
    auto last_packet_time = timeout_start;
    auto last_timing_log = std::chrono::steady_clock::time_point{};

#ifdef _WIN32
    const DWORD receive_timeout = 1000;
    setsockopt(serverUdpSocket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&receive_timeout), sizeof(receive_timeout));
#else
    const timeval receive_timeout{1, 0};
    setsockopt(serverUdpSocket, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
#endif

    while (shouldRunDataThread) {
        // Receive datagram
        std::array<u8, Falconnect::MAX_RACE_DATAGRAM_SIZE + 1> buffer{};
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);

        const int received = recvfrom(serverUdpSocket,
                                      reinterpret_cast<char*>(buffer.data()),
                                      buffer.size(),
                                      0,
                                      reinterpret_cast<sockaddr *>(&from),
                                      &fromLen);

        std::vector<Falconnect::RaceRecord> records;
        if (received < 0 || from.sin_addr.s_addr != serverUdpAddress.sin_addr.s_addr ||
            from.sin_port != serverUdpAddress.sin_port || !Falconnect::DecodeRaceDatagram(
                std::span(buffer.data(), static_cast<std::size_t>(received)), true, &records)) {
            if (std::chrono::steady_clock::now() - last_packet_time >= std::chrono::seconds(5)) {
                HandleConnectionLost();
                break;
            }
            continue;
        }

        last_packet_time = std::chrono::steady_clock::now();

        // ping spoofing lol
        // std::this_thread::sleep_for(std::chrono::milliseconds(6));

        //INFO_LOG_FMT(FALCONNECT, "DATA_FULL");
        //INFO_LOG_FMT(FALCONNECT, "Player number: {}", playerNumber);
        // Set last read frame
        if (timeBeforePing != 0) ping = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - timeBeforePing;
        //INFO_LOG_FMT(FALCONNECT, "Receiving...");

        const u32 packetNum = Falconnect::ReadRaceWord(buffer.data() + 1);

        if (hasReceivedAnyDataEver && !Falconnect::IsNewRaceSequence(packetNum, lastPacketNum)) {
            //INFO_LOG_FMT(FALCONNECT, "Skipping old packet {}", packetNum);
            continue;
        }

        lastPacketNum = packetNum;
        // INFO_LOG_FMT(FALCONNECT, "Packet {}", packetNum);

        const auto now = std::chrono::steady_clock::now();
        const bool log_timing = now - last_timing_log >= std::chrono::seconds(1);
        if (log_timing) last_timing_log = now;

        const std::lock_guard<std::mutex> assignment_lock(m_cpu_assignment_mutex);
        const std::lock_guard<std::mutex> received_data_lock(m_received_race_data_mutex);
        for (const auto& record : records) {
            const u8 local_racer = record.racer == 0 ? playerNumber - 1 : record.racer;
            if (record.racer == playerNumber - 1)
                continue;
            auto& received_racer = m_received_race_data.racers[record.racer];
            if (std::find(m_cpu_racer_indices.begin(), m_cpu_racer_indices.end(), local_racer) !=
                m_cpu_racer_indices.end()) {
                received_racer = {};
                continue;
            }
            received_racer.Apply(record, local_racer);
            if (log_timing)
                INFO_LOG_FMT(FALCONNECT, "TIMING RX player={} packet={} server_racer={} local_racer={} full={} words=[{:08x},{:08x},{:08x}]",
                             playerNumber, packetNum, record.racer, local_racer, record.full,
                             received_racer.block.timingData[0], received_racer.block.timingData[1],
                             received_racer.block.timingData[2]);
        }

        hasReceivedAnyDataEver = true;

        // std::this_thread::sleep_for(std::chrono::milliseconds(16));

        // If exited, stop everything
        if (exited) {
            // Reset everything, tell server
            hasGridded = false;
            start = false;
            exited = false;
            canLoad = false;
            shouldRunDataThread = false;
            hasReceivedAnyDataEver = false;

            char data[FULL_RACE_PACKET_SIZE] = { 0 };
            data[0] = static_cast<char>(ToServerPacketType::RESET);

            send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);

            break;
        }

        timeBeforePing = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}
