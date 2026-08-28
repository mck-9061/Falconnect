#ifndef DOLPHIN_EMU_RACERMEMORYBLOCK_H
#define DOLPHIN_EMU_RACERMEMORYBLOCK_H
#include "Common/CommonTypes.h"
#include <cstddef>
#include <vector>


class RacerMemoryBlock {
public:
    // Keep this list in lockstep with GetSocketData(). Every serialized field is a u32.
    static constexpr std::size_t SOCKET_FIELD_COUNT =
        1 +  // state
        3 +  // centerPosition
        3 +  // velocityWorld
        3 +  // orientationWorld
        3 +  // upVector
        3 +  // orientationGravity
        3 +  // speed, arialTilt, energy
        7 +  // inputs
        1 +  // sideAttack
        4 +  // maxSpeedKmh, acceleration, baseSpeed, maxSpeed
        1;   // restoreFlag
    static constexpr std::size_t SOCKET_DATA_SIZE = SOCKET_FIELD_COUNT * sizeof(u32);

    static RacerMemoryBlock* CreateFromDolphinData(const std::vector<u32> &data);

    static RacerMemoryBlock* CreateFromSocketData(const std::vector<u8> &data);

    std::vector<u8> GetSocketData() const;

    bool operator==(const RacerMemoryBlock & all_block) const {
        return
            centerPosition[0] == all_block.centerPosition[0] &&
            centerPosition[1] == all_block.centerPosition[1] &&
            centerPosition[2] == all_block.centerPosition[2] &&
            velocityWorld[0] == all_block.velocityWorld[0] &&
            velocityWorld[1] == all_block.velocityWorld[1] &&
            velocityWorld[2] == all_block.velocityWorld[2] &&
            orientationWorld[0] == all_block.orientationWorld[0] &&
            orientationWorld[1] == all_block.orientationWorld[1] &&
            orientationWorld[2] == all_block.orientationWorld[2];
    }

    u32 state;
    u32 centerPosition[3];
    u32 lastCenterPosition[3];
    u32 velocityWorld[3];
    u32 velocityMachine[3];
    u32 orientationWorld[3];
    u32 upVector[3];
    u32 orientationGravity[3];
    u32 speed;
    u32 arialTilt;
    u32 energy;
    u32 trackOrientation[3];
    u32 bottomPosition[3];
    u32 inputs[7];
    u32 sideAttack;
    u32 maxSpeedKmh;
    u32 acceleration;
    u32 baseSpeed;
    u32 maxSpeed;
    u32 restoreFlag;
    u32 centerPosOscX;
    u32 centerPosOscY;
    u32 centerPosOscZ;
};


#endif //DOLPHIN_EMU_RACERMEMORYBLOCK_H
