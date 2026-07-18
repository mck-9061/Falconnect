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
    RESET,
    READY_TO_START,
};

#endif //DOLPHIN_EMU_PACKETTYPE_H
