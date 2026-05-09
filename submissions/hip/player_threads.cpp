

#include <iostream>
#include <pthread.h>
#include <unistd.h>

#include "player_threads.h"
#include "player_actions.h"

using namespace std;

struct PlayerThreadData
{
    int          playerId;
    SharedState *state;
};

static PlayerThreadData threadData[MAX_PLAYERS];
static pthread_t        playerThreads[MAX_PLAYERS];
static int              playerThreadCount = 0;

static void *playerThreadFunction(void *arg)
{
    PlayerThreadData *data     = (PlayerThreadData *)arg;
    int               playerId = data->playerId;
    SharedState      *state    = data->state;

    cout << "[HIP] Player thread " << playerId << " started." << endl;

    while (true)
    {
        
        sem_wait(&state->stateLock);
        int status  = state->gameStatus;
        int alive   = state->players[playerId].alive;
        int isMyTurn = (state->currentTurnType == ENTITY_PLAYER &&
                        state->currentTurnId   == playerId      &&
                        alive == 1);
        sem_post(&state->stateLock);

        
        if (status != GAME_RUNNING || alive == 0)
        {
            cout << "[HIP] Player thread " << playerId
                 << " exiting (status=" << status
                 << ", alive=" << alive << ")." << endl;
            break;
        }

        if (isMyTurn)
        {
            
            cout << "[HIP] Player thread " << playerId << " is active." << endl;
            sendPlayerActionFromBuffer(state, playerId);
            
            
            usleep(10000);
        }
        else
        {
            
            usleep(100000);
        }
    }

    cout << "[HIP] Player thread " << playerId << " finished." << endl;
    return nullptr;
}


void createPlayerThreads(SharedState *state)
{
    playerThreadCount = state->playerCount;

    for (int i = 0; i < playerThreadCount; i++)
    {
        threadData[i].playerId = i;
        threadData[i].state    = state;

        
        pthread_create(&playerThreads[i], nullptr,
                       playerThreadFunction, &threadData[i]);
    }

    cout << "[HIP] Created " << playerThreadCount
         << " player thread(s)." << endl;
}


void joinPlayerThreads(SharedState *state)
{
    
    for (int i = 0; i < playerThreadCount; i++)
        sem_post(&state->actionDone);

    for (int i = 0; i < playerThreadCount; i++)
    {
        pthread_join(playerThreads[i], nullptr);
        cout << "[HIP] Player thread " << i << " joined." << endl;
    }
}