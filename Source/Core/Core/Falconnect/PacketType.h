#ifndef DOLPHIN_EMU_PACKETTYPE_H
#define DOLPHIN_EMU_PACKETTYPE_H
#include "Common/CommonTypes.h"

enum class PacketType : char {
    RACER_ID,
    PING,
    DATA_FULL,
    READY,
    START_RACE,
    LOADED,
    COUNTDOWN,
};

#endif //DOLPHIN_EMU_PACKETTYPE_H
