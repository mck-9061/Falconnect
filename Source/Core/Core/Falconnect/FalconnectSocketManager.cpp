#include "FalconnectSocketManager.h"

#include <algorithm>
#include <iterator>
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

        //inet_pton(AF_INET, "162.19.231.212", &serverAddress.sin_addr); // Remote IP address
        inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr); // Remote IP address

        code = connect(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress));
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (code != -1) break;
    }
    INFO_LOG_FMT(FALCONNECT, "Code: {}", code);

    if (code < 0) {
        isError = true;
    } else {
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

        for (std::size_t i = 0; i < std::size(usedIndices); i++)
        {
            if (usedIndices[i] == racer)
            {
                usedIndices[i] = 0;
                updated[i] = false;
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
    INFO_LOG_FMT(FALCONNECT, "CPU assignment updated: {} CPU racers", ourCpus);
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

    INFO_LOG_FMT(FALCONNECT, "Lost connection to Falconnect server");
    shouldRunDataThread = false;
    shouldDisconnect = true;
    shouldRun = false;
    isError = true;

    if (FalconnectManager::instance != nullptr)
    {
        FalconnectManager::instance->shouldDisplayDisconnectedAlert = true;
        FalconnectManager::instance->shouldReset = true;
    }
}

void FalconnectSocketManager::SocketThread() {
    Start();

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
                INFO_LOG_FMT(FALCONNECT, "Disconnecting!");
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
                INFO_LOG_FMT(FALCONNECT, "Dropping invalid packet!");
                  invalidPacketCount++;

                  if (invalidPacketCount > 50) {
                      // Server probably closed, or we lost connection
                      INFO_LOG_FMT(FALCONNECT, "Disconnecting!");
                      {
                          FalconnectManager::instance->shouldDisplayDisconnectedAlert = true;
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

                inet_pton(AF_INET, "127.0.0.1", &serverUdpAddress.sin_addr); // Remote IP address
                //inet_pton(AF_INET, "162.19.231.212", &serverUdpAddress.sin_addr); // Remote IP address

                // connect(serverUdpSocket, reinterpret_cast<struct sockaddr *>(&serverUdpAddress), sizeof(serverUdpAddress));

                // Send our name
                char data1[FULL_RACE_PACKET_SIZE] = { 0 };
                data1[0] = static_cast<char>(ToServerPacketType::NAME);

                for (std::size_t i = 0; i < 32 && i < name.size(); i++) {
                    data1[i + 1] = static_cast<char>(name[i]);
                }

                send(serverSocket, data1, FULL_RACE_PACKET_SIZE, 0);

                // Connected: Wait for us to be ready
                while (!canLoad) {
                    INFO_LOG_FMT(FALCONNECT, "Waiting until we can load...");
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    if (shouldDisconnect) {
                        break;
                    }
                }

                if (shouldDisconnect) {
                    break;
                }

                // First send our selected racer ID, then say we're ready
                char data[FULL_RACE_PACKET_SIZE] = { 0 };
                data[0] = static_cast<char>(ToServerPacketType::SETTINGS);
                data[1] = static_cast<char>(racerId);
                data[2] = static_cast<char>(selectedCourse);

                send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);

                char data2[FULL_RACE_PACKET_SIZE] = { 0 };
                data2[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
                data2[1] = static_cast<char>(ClientState::READY);
                data2[2] = 1;

                send(serverSocket, data2, FULL_RACE_PACKET_SIZE, 0);

                break;
            }

            default:
                invalidPacketCount = 0;

            case FromServerPacketType::COURSE: {
                usedCourseId = buffer[1];
                cpuCount = buffer[2];

                break;
            }

            case FromServerPacketType::RACER_IDS: {
                // Construct array
                for (u8 i = 0; i < 30; i++) {
                    u8 usedIndex = i;
                    if (i == 0) usedIndex = playerNumber - 1;
                    if (i == playerNumber - 1) {
                        INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);
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
                    // char name[32];
                    // std::memcpy(name, buffer + 1 + (i * 32), 32);
                    //
                    // std::vector<u8> iHateCpp{};
                    //
                    // iHateCpp.reserve(32);
                    // for (const char j : name) {
                    //     iHateCpp.push_back(j);
                    // }

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

            case FromServerPacketType::START: {
                INFO_LOG_FMT(FALCONNECT, "START");
                exited = false;

                { // force the compiler to not be a whiny baby
                  FalconnectManager::instance->shouldStart = true;
                }

                std::this_thread::sleep_for(std::chrono::seconds(5));

                // Tell the server when we've gridded
                while (!hasGridded) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    if (shouldDisconnect) {
                        break;
                    }
                }

                if (shouldDisconnect) {
                    break;
                }

                char data[FULL_RACE_PACKET_SIZE] = { 0 };
                data[0] = static_cast<char>(ToServerPacketType::UPDATE_STATE);
                data[1] = static_cast<char>(ClientState::GRIDDED);
                data[2] = 1;

                send(serverSocket, data, FULL_RACE_PACKET_SIZE, 0);

                break;
            }

            case FromServerPacketType::START_RACE: {
                INFO_LOG_FMT(FALCONNECT, "START_RACE");
                start = true;

                // Each server race starts its UDP sequence at zero. Do not reject the new race's
                // first packets using the previous race's sequence number.
                lastPacketNum = 0;
                hasReceived = false;
                hasReceivedAnyDataEver = false;
                timeBeforePing = 0;

                std::this_thread::sleep_for(std::chrono::seconds(1));

                shouldRunDataThread = true;
                std::thread readThread(&FalconnectSocketManager::DataThread, this);
                readThread.detach();

                std::thread memThread(&FalconnectSocketManager::MemoryThread, this);
                memThread.detach();

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

void FalconnectSocketManager::MemoryThread() const {
    u32 timeBeforeUpdate = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // while (shouldRunDataThread) {
    //     if (FalconnectManager::instance->currentState == GameState::RACING) {
    //         for (int i = 0; i < 30; i++) {
    //             if (const auto racerNum = instance->usedIndices[i]; racerNum != 0) {
    //                 const auto racerBlock = instance->allBlocks[i];
    //
    //                 FalconnectManager::instance->patcher->SetRacerData(racerNum, *racerBlock, true);
    //                 //FalconnectManager::instance->lastWrittenBlocks[racerNum - 1] = racerBlock;
    //                 //INFO_LOG_FMT(FALCONNECT, "Racer data set");
    //             }
    //         }
    //     }
    // }
}

void FalconnectSocketManager::SendDataThread() {
    u32 sentCount = 0;
    u32 timeBeforeUpdate = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    while (shouldRunDataThread) {
        // while ((!shouldSendData) && hasReceivedAnyDataEver) {
        //     std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // }
        // shouldSendData = false;

        int sendPing = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - timeBeforeUpdate;
        INFO_LOG_FMT(FALCONNECT, "Send delay: {}", sendPing);
        timeBeforeUpdate = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        if (FalconnectManager::instance->currentState != GameState::RACING) continue;
        // Send our frames
        sentCount++;

        // instance->SendFrame(FalconnectManager::instance->patcher->memoryReader->ReadRacerData(0), 0);
        //
        // for (int i = 0; i < instance->ourCpus; i++) {
        //     //INFO_LOG_FMT(FALCONNECT, "Reading racer at index {}", FalconnectSocketManager::instance->cpuStartIndex + i);
        //     instance->SendFrame(FalconnectManager::instance->patcher->memoryReader->ReadRacerData(instance->cpuStartIndex + i), i + 1);
        // }

        while (framesToSend[0] == nullptr) {
            INFO_LOG_FMT(FALCONNECT, "Bad frame!");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        //INFO_LOG_FMT(FALCONNECT, "Sending...");

        // Send frame to be sent to remote
        lockFrameToSend = true;

        char data[FULL_RACE_PACKET_SIZE] = { 0 };
        data[0] = static_cast<char>(FromServerPacketType::FULL_DATA);

        data[1] = static_cast<char>((sentCount >> 24) & 0xff);
        data[2] = static_cast<char>((sentCount >> 16) & 0xff);
        data[3] = static_cast<char>((sentCount >> 8) & 0xff);
        data[4] = static_cast<char>(sentCount & 0xff);

        const auto cpu_racer_indices = GetCpuRacerIndices();
        const std::size_t racer_count = cpu_racer_indices.size() + 1;
        bool missing_frame = false;
        for (std::size_t frame_index = 0; frame_index < racer_count; frame_index++) {
            if (framesToSend[frame_index] == nullptr) {
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

            i += RacerMemoryBlock::SOCKET_DATA_SIZE;
            j++;
        }

        lockFrameToSend = false;

        sendto(serverUdpSocket,
            data,
            RacePacketSize(racer_count),
            0,
            reinterpret_cast<sockaddr *>(&serverUdpAddress),
            sizeof(serverUdpAddress));

        //INFO_LOG_FMT(FALCONNECT, "Sent");

        //if (!hasReceivedAnyDataEver) std::this_thread::sleep_for(std::chrono::milliseconds(4));
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
}

void FalconnectSocketManager::DataThread() {
    const auto timeout_start = std::chrono::steady_clock::now();
    auto last_packet_time = timeout_start;

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
        char buffer[FULL_RACE_PACKET_SIZE] = { 0 };
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);

        const int received = recvfrom(serverUdpSocket,
                                      buffer,
                                      sizeof(buffer),
                                      0,
                                      reinterpret_cast<sockaddr *>(&from),
                                      &fromLen);

        if (received != FULL_RACE_PACKET_SIZE ||
            buffer[0] != static_cast<char>(FromServerPacketType::FULL_DATA)) {
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

        const u32 packetNum =
            ((buffer[1] & 0xff) << 24) |
                ((buffer[2] & 0xff) << 16) |
                    ((buffer[3] & 0xff) << 8) |
                        (buffer[4] & 0xff);

        if (packetNum < lastPacketNum) {
            //INFO_LOG_FMT(FALCONNECT, "Skipping old packet {}", packetNum);
            continue;
        }

        lastPacketNum = packetNum;
        //INFO_LOG_FMT(FALCONNECT, "Packet {}", packetNum);

        for (u8 i = 0; i < 30; i++) {
            u8 usedIndex = i;
            if (i == 0) usedIndex = playerNumber - 1;
            if (i == playerNumber - 1) {
              //INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our data", i);

                // Store the server's last known position of us
                const std::vector<u8> racerData(
                    buffer + RACE_PACKET_HEADER_SIZE + (i * RacerMemoryBlock::SOCKET_DATA_SIZE),
                    buffer + RACE_PACKET_HEADER_SIZE + ((i + 1) * RacerMemoryBlock::SOCKET_DATA_SIZE));
                ourLastKnownData = RacerMemoryBlock::CreateFromSocketData(racerData);

                continue; // Skip our data
            }

            if (OwnsCpuRacer(usedIndex)) {
                //INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Our CPU", i);
                continue;
            }

            //INFO_LOG_FMT(FALCONNECT, "Storing racer at index {} in slot {}", i, usedIndex);

            if (const std::vector<u8> racerData(
                    buffer + RACE_PACKET_HEADER_SIZE + (i * RacerMemoryBlock::SOCKET_DATA_SIZE),
                    buffer + RACE_PACKET_HEADER_SIZE + ((i + 1) * RacerMemoryBlock::SOCKET_DATA_SIZE));
                racerData[0] == 0x00) {
                //INFO_LOG_FMT(FALCONNECT, "Skipping racer at index {}: Invalid data", i);
            } else {
                RacerMemoryBlock* block = RacerMemoryBlock::CreateFromSocketData(racerData);
                // operationQueue.push(OperationType::SET_RACER_BLOCK);
                // operationArgumentsQueue.emplace(usedIndex);
                // operationArgumentsQueue.emplace(*block);

                if (allBlocks[i] != nullptr) updated[i] = *block != *allBlocks[i];
                else updated[i] = true;

                allBlocks[i] = block;
                usedIndices[i] = usedIndex;

                // if (updated[i]) {
                //     FalconnectManager::instance->patcher->SetRacerData(usedIndex, *block, true);
                // } else {
                //     FalconnectManager::instance->patcher->SetRacerData(usedIndex, *block, false);
                // }
            }
        }

        hasReceived = true;
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
