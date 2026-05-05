#ifndef DOLPHIN_EMU_RACERMEMORYBLOCK_H
#define DOLPHIN_EMU_RACERMEMORYBLOCK_H
#include "Common/CommonTypes.h"
#include <vector>


class RacerMemoryBlock {
public:
    static RacerMemoryBlock* CreateFromDolphinData(const std::vector<u32> &data);

    static RacerMemoryBlock* CreateFromSocketData(const std::vector<u8> &data);

    std::vector<u8> GetSocketData() const;

    std::vector<u8> GetDolphinPatchData(std::vector<u32> currentData);

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
    u32 inputs[3];
    u32 sideAttack;
};


#endif //DOLPHIN_EMU_RACERMEMORYBLOCK_H
