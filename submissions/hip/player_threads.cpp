#include <iostream>
#include <pthread.h>
#include <unistd.h>

#include "player_threads.h"
#include "player_actions.h"

using namespace std;

struct PlayerThreadData {
    int playerId;
    SharedState* state;
};

PlayerThreadData threadData[MAX_PLAYERS];
pthread_t playerThreads[MAX_PLAYERS];

void* playerThreadFunction(void* arg) {
    PlayerThreadData* data = (PlayerThreadData*)arg;

    int playerId = data->playerId;
    SharedState* state = data->state;

    cout << "Player thread " << playerId << " created." << endl;

    while (state->gameStatus == GAME_RUNNING) {
        sem_wait(&state->stateLock);

        int isMyTurn = 0;

        if (state->currentTurnType == ENTITY_PLAYER &&
            state->currentTurnId == playerId &&
            state->players[playerId].alive == 1) {
            isMyTurn = 1;
        }

        sem_post(&state->stateLock);

        if (isMyTurn == 1) {
            cout << "Player thread " << playerId << " is active now." << endl;

            sendPlayerActionFromBuffer(state, playerId);

            usleep(100000);
        }
        else {
            usleep(100000);
        }
    }

    cout << "Player thread " << playerId << " exiting." << endl;

    return NULL;
}

void createPlayerThreads(SharedState* state) {
    int count = state->playerCount;

    for (int i = 0; i < count; i++) {
        threadData[i].playerId = i;
        threadData[i].state = state;

        pthread_create(&playerThreads[i], NULL, playerThreadFunction, &threadData[i]);
        pthread_detach(playerThreads[i]);
    }
}