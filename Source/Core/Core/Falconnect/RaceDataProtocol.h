// // Copyright 2026 Dolphin Emulator Project
// // SPDX-License-Identifier: GPL-2.0-or-later
// #pragma once
//
// #include <array>
// #include <memory>
// #include <span>
// #include <vector>
//
// #include "Core/Falconnect/RacerMemoryBlock.h"
//
// namespace Falconnect
// {
// inline constexpr std::size_t MAX_RACERS = 30;
// inline constexpr std::size_t MAX_RACE_DATAGRAM_SIZE = 5 + MAX_RACERS * (6 + RacerMemoryBlock::SOCKET_DATA_SIZE);
//
// inline void AppendRaceWord(std::vector<u8>& bytes, u32 word)
// {
//     for (int shift = 24; shift >= 0; shift -= 8)
//         bytes.push_back(static_cast<u8>(word >> shift));
// }
//
// inline u32 ReadRaceWord(const u8* bytes)
// {
//     return (u32{bytes[0]} << 24) | (u32{bytes[1]} << 16) | (u32{bytes[2]} << 8) | bytes[3];
// }
//
// inline bool IsNewRaceSequence(u32 next, u32 previous, u32 mask = 0xffffffff)
// {
//     const u32 distance = (next - previous) & mask;
//     return distance != 0 && distance <= mask / 2;
// }
//
// struct RaceRecord
// {
//     u8 racer = 0;
//     bool full = false;
//     u32 source = 0;
//     std::vector<u8> payload;
// };
//
// // Validate the entire datagram before publishing any records. UDP records use explicit global
// // racer IDs so a CPU assignment changing while a frame is sampled cannot relabel that frame.
// inline bool DecodeRaceDatagram(std::span<const u8> bytes, bool from_server,
//                                std::vector<RaceRecord>* records)
// {
//     records->clear();
//     if (bytes.size() < 5 || bytes.size() > MAX_RACE_DATAGRAM_SIZE || bytes[0] != 1)
//         return false;
//     std::array<bool, MAX_RACERS> seen{};
//     const std::size_t header_size = from_server ? 6 : 2;
//     for (std::size_t offset = 5; offset < bytes.size();)
//     {
//         if (bytes.size() - offset < header_size)
//             return false;
//         const u8 racer = bytes[offset];
//         const u8 kind = bytes[offset + 1];
//         if (racer >= MAX_RACERS || seen[racer] || kind > 1)
//             return false;
//         seen[racer] = true;
//         const std::size_t size = kind ? RacerMemoryBlock::SOCKET_DATA_SIZE : RacerMemoryBlock::INPUT_DATA_SIZE;
//         if (bytes.size() - offset - header_size < size)
//             return false;
//         records->push_back({racer, kind == 1,
//             from_server ? ReadRaceWord(bytes.data() + offset + 2) : ReadRaceWord(bytes.data() + 1),
//             std::vector<u8>(bytes.begin() + offset + header_size,
//                            bytes.begin() + offset + header_size + size)});
//         offset += header_size + size;
//     }
//     return true;
// }
//
// struct ReceivedRacerData
// {
//     RacerMemoryBlock block{};
//     u8 racer_index = 0;
//     u32 source_sequence = 0;
//     bool valid = false;
//     bool full_updated = false;
//
//     void Apply(const RaceRecord& record, u8 local_racer)
//     {
//         const bool same_owner = valid && (source_sequence >> 24) == (record.source >> 24);
//         if (same_owner && !IsNewRaceSequence(record.source, source_sequence, 0x00ffffff))
//             return;
//         if (!same_owner)
//         {
//             block = {};
//             full_updated = false;
//         }
//         if (record.full)
//         {
//             const std::unique_ptr<RacerMemoryBlock> full(RacerMemoryBlock::CreateFromSocketData(record.payload));
//             block = *full;
//             full_updated = true;
//         }
//         else
//         {
//             // Keep a pending full correction, but overlay the newest controls before applying it.
//             block.ApplyInputSocketData(record.payload);
//         }
//         racer_index = local_racer;
//         source_sequence = record.source;
//         valid = true;
//     }
// };
// }  // namespace Falconnect
