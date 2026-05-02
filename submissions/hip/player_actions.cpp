#include <iostream>
#include <unistd.h>

#include "player_actions.h"
#include "input_buffer.h"

using namespace std;

void sendPlayerActionFromBuffer(SharedState* state, int playerId) {
    int actionType = ACTION_NONE;
    int targetType = ENTITY_NONE;
    int targetId = -1;

    cout << "Player thread " << playerId << " waiting for input buffer..." << endl;

    while (state->gameStatus == GAME_RUNNING) {
        int gotInput = readInputBuffer(state, playerId, actionType, targetType, targetId);

        if (gotInput == 1) {
            break;
        }

        usleep(100000);
    }

    if (state->gameStatus != GAME_RUNNING) {
        return;
    }

    sem_wait(&state->stateLock);

    state->request.ready = 1;
    state->request.entityType = ENTITY_PLAYER;
    state->request.entityId = playerId;
    state->request.actionType = actionType;
    state->request.targetType = targetType;
    state->request.targetId = targetId;

    sem_post(&state->stateLock);

    cout << "Player thread " << playerId << " sent action request to Arbiter." << endl;

    sem_post(&state->actionReady);
    sem_wait(&state->actionDone);

    cout << "Arbiter processed Player " << playerId << " action." << endl;
}