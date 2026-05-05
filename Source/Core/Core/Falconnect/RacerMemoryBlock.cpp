#include "RacerMemoryBlock.h"

RacerMemoryBlock* RacerMemoryBlock::CreateFromDolphinData(const std::vector<u32> &data) {
    auto *block = new RacerMemoryBlock();

    block->state = data[0];

    block->centerPosition[0] = data[31];
    block->centerPosition[1] = data[32];
    block->centerPosition[2] = data[33];

    block->lastCenterPosition[0] = data[34]; // also: 0x1e0:0x1ec, 0x4c8:0x4d4
    block->lastCenterPosition[1] = data[35];
    block->lastCenterPosition[2] = data[36];

    block->velocityWorld[0] = data[37];
    block->velocityWorld[1] = data[38];
    block->velocityWorld[2] = data[39];

    block->velocityMachine[0] = data[46]; // Z component also stored at 0xd4:0xd8
    block->velocityMachine[1] = data[47];
    block->velocityMachine[2] = data[48];

    block->orientationWorld[0] = data[59]; // also: 0x11c:0x128, 0x14c:0x158
    block->orientationWorld[1] = data[60];
    block->orientationWorld[2] = data[61];

    block->upVector[0] = data[63]; // also: 0x12c:0x138, 0x15c:0x168
    block->upVector[1] = data[64];
    block->upVector[2] = data[65];

    block->orientationGravity[0] = data[67]; // also: 0x13c:0x148, 0x16c:0x178
    block->orientationGravity[1] = data[68];
    block->orientationGravity[2] = data[69];

    block->speed = data[95];
    block->arialTilt = data[96];
    block->energy = data[97];

    block->trackOrientation[0] = data[111];
    block->trackOrientation[1] = data[112];
    block->trackOrientation[2] = data[113];

    block->bottomPosition[0] = data[117];
    block->bottomPosition[1] = data[118];
    block->bottomPosition[2] = data[119];

    block->inputs[0] = data[123]; // l/r also stored at 0x20c:0x210 (0x1fc:0x200)
    block->inputs[1] = data[124];
    block->inputs[2] = data[125];

    block->sideAttack = data[388];
    return block;
}

RacerMemoryBlock* RacerMemoryBlock::CreateFromSocketData(const std::vector<u8> &data) {
    // Convert u8 vector to u32 (big endian)
    std::vector<uint32_t> usedData;
    usedData.reserve(data.size() / 4);

    for (size_t i = 0; i < data.size(); i += 4) {
        uint32_t value =
            (static_cast<uint32_t>(data[i])     << 24) |
            (static_cast<uint32_t>(data[i + 1]) << 16) |
            (static_cast<uint32_t>(data[i + 2]) << 8)  |
            (static_cast<uint32_t>(data[i + 3]));

        usedData.push_back(value);
    }

    auto *block = new RacerMemoryBlock();

    block->state = usedData[0];

    block->centerPosition[0] = usedData[1];
    block->centerPosition[1] = usedData[2];
    block->centerPosition[2] = usedData[3];

    block->lastCenterPosition[0] = usedData[4]; // also: 0x1e0:0x1ec, 0x4c8:0x4d4
    block->lastCenterPosition[1] = usedData[5];
    block->lastCenterPosition[2] = usedData[6];

    block->velocityWorld[0] = usedData[7];
    block->velocityWorld[1] = usedData[8];
    block->velocityWorld[2] = usedData[9];

    block->velocityMachine[0] = usedData[10]; // Z component also stored at 0xd4:0xd8
    block->velocityMachine[1] = usedData[11];
    block->velocityMachine[2] = usedData[12];

    block->orientationWorld[0] = usedData[13]; // also: 0x11c:0x128, 0x14c:0x158
    block->orientationWorld[1] = usedData[14];
    block->orientationWorld[2] = usedData[15];

    block->upVector[0] = usedData[16]; // also: 0x12c:0x138, 0x15c:0x168
    block->upVector[1] = usedData[17];
    block->upVector[2] = usedData[18];

    block->orientationGravity[0] = usedData[19]; // also: 0x13c:0x148, 0x16c:0x178
    block->orientationGravity[1] = usedData[20];
    block->orientationGravity[2] = usedData[21];

    block->speed = usedData[22];
    block->arialTilt = usedData[23];
    block->energy = usedData[24];

    block->trackOrientation[0] = usedData[25];
    block->trackOrientation[1] = usedData[26];
    block->trackOrientation[2] = usedData[27];

    block->bottomPosition[0] = usedData[28];
    block->bottomPosition[1] = usedData[29];
    block->bottomPosition[2] = usedData[30];

    block->inputs[0] = usedData[31]; // l/r also stored at 0x20c:0x210 (0x1fc:0x200)
    block->inputs[1] = usedData[32];
    block->inputs[2] = usedData[33];

    block->sideAttack = usedData[34];

    return block;
}

std::vector<u8> RacerMemoryBlock::GetSocketData() const {
    std::vector<u32> data;

    data.push_back(state);

    data.push_back(centerPosition[0]);
    data.push_back(centerPosition[1]);
    data.push_back(centerPosition[2]);

    data.push_back(lastCenterPosition[0]);
    data.push_back(lastCenterPosition[1]);
    data.push_back(lastCenterPosition[2]);

    data.push_back(velocityWorld[0]);
    data.push_back(velocityWorld[1]);
    data.push_back(velocityWorld[2]);

    data.push_back(velocityMachine[0]);
    data.push_back(velocityMachine[1]);
    data.push_back(velocityMachine[2]);

    data.push_back(orientationWorld[0]);
    data.push_back(orientationWorld[1]);
    data.push_back(orientationWorld[2]);

    data.push_back(upVector[0]);
    data.push_back(upVector[1]);
    data.push_back(upVector[2]);

    data.push_back(orientationGravity[0]);
    data.push_back(orientationGravity[1]);
    data.push_back(orientationGravity[2]);

    data.push_back(speed);
    data.push_back(arialTilt);
    data.push_back(energy);

    data.push_back(trackOrientation[0]);
    data.push_back(trackOrientation[1]);
    data.push_back(trackOrientation[2]);

    data.push_back(bottomPosition[0]);
    data.push_back(bottomPosition[1]);
    data.push_back(bottomPosition[2]);

    data.push_back(inputs[0]);
    data.push_back(inputs[1]);
    data.push_back(inputs[2]);

    data.push_back(sideAttack);

    // convert to u8 vector
    std::vector<uint8_t> out;
    out.reserve(data.size() * 4);

    for (const uint32_t v : data) {
        out.push_back((v >> 24) & 0xFF);
        out.push_back((v >> 16) & 0xFF);
        out.push_back((v >> 8)  & 0xFF);
        out.push_back(v & 0xFF);
    }

    return out;
}

std::vector<u8> RacerMemoryBlock::GetDolphinPatchData(std::vector<u32> currentData) {

}
