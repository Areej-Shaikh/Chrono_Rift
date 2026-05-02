#include "input_buffer.h"

void writeInputBuffer(SharedState* state, int playerId, int actionType, int targetType, int targetId) {
    sem_wait(&state->stateLock);

    state->inputBuffer.hasInput = 1;
    state->inputBuffer.playerId = playerId;
    state->inputBuffer.actionType = actionType;
    state->inputBuffer.targetType = targetType;
    state->inputBuffer.targetId = targetId;

    sem_post(&state->stateLock);
}

int readInputBuffer(SharedState* state, int playerId, int& actionType, int& targetType, int& targetId) {
    int found = 0;

    sem_wait(&state->stateLock);

    if (state->inputBuffer.hasInput == 1 &&
        state->inputBuffer.playerId == playerId) {

        actionType = state->inputBuffer.actionType;
        targetType = state->inputBuffer.targetType;
        targetId = state->inputBuffer.targetId;

        state->inputBuffer.hasInput = 0;
        state->inputBuffer.playerId = -1;
        state->inputBuffer.actionType = ACTION_NONE;
        state->inputBuffer.targetType = ENTITY_NONE;
        state->inputBuffer.targetId = -1;

        found = 1;
    }

    sem_post(&state->stateLock);

    return found;
}