#ifndef DOLPHIN_EMU_PACKETTYPE_H
#define DOLPHIN_EMU_PACKETTYPE_H
#include "Common/CommonTypes.h"

enum class FromServerPacketType : char {
    CONNECTED,
    FULL_DATA,
    START,
    START_RACE,
    RACER_IDS,
    COURSE,
    NAMES
};

enum class ToServerPacketType : char {
    UPDATE_STATE,
    FULL_DATA,
    SETTINGS,
    RESET,
    DISCONNECT,
    NAME
};

#endif //DOLPHIN_EMU_PACKETTYPE_H
