#ifndef INPUT_BUFFER_H
#define INPUT_BUFFER_H

#include "shared_state.h"

void writeInputBuffer(SharedState* state, int playerId, int actionType, int targetType, int targetId);
int readInputBuffer(SharedState* state, int playerId, int& actionType, int& targetType, int& targetId);

#endif