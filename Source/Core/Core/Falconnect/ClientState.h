#ifndef DOLPHIN_EMU_CLIENTSTATE_H
#define DOLPHIN_EMU_CLIENTSTATE_H

enum class ClientState : char {
    IN_MENUS,
    READY,
    LOADING,
    GRIDDED,
    RACING
};

#endif //DOLPHIN_EMU_CLIENTSTATE_H
