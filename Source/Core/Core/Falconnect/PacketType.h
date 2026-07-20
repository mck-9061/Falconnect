#ifndef DOLPHIN_EMU_PACKETTYPE_H
#define DOLPHIN_EMU_PACKETTYPE_H
#include "Common/CommonTypes.h"

enum class FromServerPacketType : char {
    CONNECTED,
    FULL_DATA,
    START,
    START_RACE,
};

enum class ToServerPacketType : char {
    UPDATE_STATE,
    FULL_DATA
};

#endif //DOLPHIN_EMU_PACKETTYPE_H
