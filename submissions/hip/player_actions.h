#ifndef PLAYER_ACTIONS_H
#define PLAYER_ACTIONS_H

#include "shared_state.h"

void sendPlayerActionFromBuffer(SharedState* state, int playerId);

#endif