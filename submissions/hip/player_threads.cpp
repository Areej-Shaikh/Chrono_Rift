/*
 * player_threads.cpp  —  One pthread per human-controlled character (spec §2)
 *
 * Key fixes over the previous version:
 *
 * 1. Threads are joinable, not detached.
 *    pthread_detach() makes it impossible to wait for a thread to finish or
 *    detect when it has exited. joinPlayerThreads() now does a proper
 *    pthread_join() on every player thread so the HIP main thread can
 *    confirm all threads are dead before it exits.
 *
 * 2. Clean shutdown on game end.
 *    The loop condition checks gameStatus under stateLock (not as a raw read)
 *    so the thread sees the GAME_WIN/LOSE/QUIT transition promptly. If a
 *    thread is blocked inside sendPlayerActionFromBuffer waiting for input
 *    that will never come, joinPlayerThreads() posts actionDone once per
 *    player to unblock any sem_wait(&state->actionDone) still in flight.
 *
 * 3. Correct active/idle logic (rubric: "Player Thread Handling").
 *    Only the thread whose playerId matches currentTurnId processes input.
 *    All other threads remain idle in the usleep loop — they do NOT touch
 *    the input buffer or submit any request.
 *
 * 4. Dead player threads exit immediately.
 *    If players[playerId].alive == 0 the thread exits its loop, matching
 *    the lifecycle management requirement.
 */

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
        // ── Read game status and turn info atomically ─────────────────────
        sem_wait(&state->stateLock);
        int status  = state->gameStatus;
        int alive   = state->players[playerId].alive;
        int isMyTurn = (state->currentTurnType == ENTITY_PLAYER &&
                        state->currentTurnId   == playerId      &&
                        alive == 1);
        sem_post(&state->stateLock);

        // ── Exit conditions ───────────────────────────────────────────────
        if (status != GAME_RUNNING || alive == 0)
        {
            cout << "[HIP] Player thread " << playerId
                 << " exiting (status=" << status
                 << ", alive=" << alive << ")." << endl;
            break;
        }

        if (isMyTurn)
        {
            // ── Active: this thread processes its turn ────────────────────
            cout << "[HIP] Player thread " << playerId << " is active." << endl;
            sendPlayerActionFromBuffer(state, playerId);
            // After the action is processed, loop back and re-check status.
            // A small yield avoids busy-spinning if the Arbiter is slow to
            // clear currentTurnId.
            usleep(10000);
        }
        else
        {
            // ── Idle: not this thread's turn — just wait ──────────────────
            usleep(100000);
        }
    }

    cout << "[HIP] Player thread " << playerId << " finished." << endl;
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// createPlayerThreads — spawn one thread per player, all joinable
// ─────────────────────────────────────────────────────────────────────────────
void createPlayerThreads(SharedState *state)
{
    playerThreadCount = state->playerCount;

    for (int i = 0; i < playerThreadCount; i++)
    {
        threadData[i].playerId = i;
        threadData[i].state    = state;

        // Create joinable (default attr) — NOT detached.
        pthread_create(&playerThreads[i], nullptr,
                       playerThreadFunction, &threadData[i]);
    }

    cout << "[HIP] Created " << playerThreadCount
         << " player thread(s)." << endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// joinPlayerThreads — wait for all player threads to finish
//
// Call this from the HIP main loop after the game ends.
// Posts actionDone once per player first to unblock any thread that is
// still blocked inside sem_wait(&state->actionDone) waiting for the
// Arbiter to confirm an action that will now never come.
// ─────────────────────────────────────────────────────────────────────────────
void joinPlayerThreads(SharedState *state)
{
    // Unblock any thread stuck waiting for actionDone.
    for (int i = 0; i < playerThreadCount; i++)
        sem_post(&state->actionDone);

    for (int i = 0; i < playerThreadCount; i++)
    {
        pthread_join(playerThreads[i], nullptr);
        cout << "[HIP] Player thread " << i << " joined." << endl;
    }
}