/*
 * enemy_threads.cpp  —  Per-NPC pthread logic
 *
 * Flow per thread:
 *   1. Poll until currentTurnType == ENTITY_ENEMY && currentTurnId == myId.
 *   2. Check per-enemy stunned flag in shared memory (set by SIGUSR1 handler).
 *      If stunned: sleep 3 s, clear flag, submit SKIP, wait actionDone.
 *   3. Otherwise: call submitEnemyMove(), then sem_wait(actionDone).
 *      The actionDone wait is CRITICAL — without it the thread immediately
 *      polls again, sees currentTurnId still set, and submits a duplicate.
 *   4. Loop back to 1.
 *
 * The old implementation used a global aspStunned flag that blocked ALL enemy
 * threads. The spec says stun targets a specific entity, not the whole process.
 * Per-enemy stunned is stored in state->enemies[id].stunned (shared_state.h).
 */

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "enemy_threads.h"
#include "enemy_actions.h"
#include "enemy_signals.h"

struct EnemyThreadArg
{
    SharedState *state;
    int enemyId;
};

static EnemyThreadArg *g_enemyThreadArgs = nullptr;

static void *enemyThreadFunction(void *arg)
{
    EnemyThreadArg *threadArg = (EnemyThreadArg *)arg;
    SharedState *state = threadArg->state;
    int enemyId = threadArg->enemyId;

    // ── Mark this thread as alive in shared memory ────────────────────────
    sem_wait(&state->stateLock);
    state->npcThreadAlive[enemyId] = 1;
    sem_post(&state->stateLock);

    while (true)
    {
        // ── Check game status and own liveness ────────────────────────────
        sem_wait(&state->stateLock);
        int status = state->gameStatus;
        int alive = state->enemies[enemyId].alive;
        sem_post(&state->stateLock);

        if (status != GAME_RUNNING || alive == 0)
        {
            break;
        }

        // ── Wait for our turn ─────────────────────────────────────────────
        sem_wait(&state->stateLock);
        int myTurn = (state->currentTurnType == ENTITY_ENEMY &&
                      state->currentTurnId == enemyId);
        sem_post(&state->stateLock);

        if (myTurn == 0)
        {
            usleep(50000);
            continue;
        }

        // ── Check per-enemy stun (spec Section 5) ─────────────────────────
        // SIGUSR1 handler sets state->enemies[id].stunned = 1.
        sem_wait(&state->stateLock);
        int wasStunned = state->enemies[enemyId].stunned;
        sem_post(&state->stateLock);

        if (wasStunned == 1)
        {
            std::cout << "[ASP] Enemy " << enemyId
                      << " is stunned. Pausing 3 seconds." << std::endl;

            // Sleep exactly 3 seconds (spec: non-polling, non-blocking)
            struct timespec ts = {3, 0};
            nanosleep(&ts, nullptr);

            sem_wait(&state->stateLock);
            state->enemies[enemyId].stunned = 0;
            // Stamina preserved — submit SKIP to consume turn
            state->request.ready = 1;
            state->request.entityType = ENTITY_ENEMY;
            state->request.entityId = enemyId;
            state->request.actionType = ACTION_SKIP;
            state->request.targetType = ENTITY_NONE;
            state->request.targetId = -1;
            sem_post(&state->stateLock);

            sem_post(&state->actionReady);
            sem_wait(&state->actionDone);

            std::cout << "[ASP] Enemy " << enemyId
                      << " stun ended, turn skipped." << std::endl;
            continue;
        }

        // ── Submit action ─────────────────────────────────────────────────
        submitEnemyMove(enemyId, state);

        // ── CRITICAL: wait for Arbiter to apply the action ─────────────────
        // Without this, the thread loops back immediately, sees currentTurnId
        // still set (Arbiter hasn't cleared it yet), and fires a duplicate
        // request. That duplicate stomps the first one — enemy appears to act
        // but player HP never changes.
        sem_wait(&state->actionDone);
    }

    // ── Mark this thread as dead before exiting ───────────────────────────
    sem_wait(&state->stateLock);
    state->npcThreadAlive[enemyId] = 0;
    sem_post(&state->stateLock);

    std::cout << "[ASP] Enemy thread " << enemyId << " exiting." << std::endl;
    return nullptr;
}

void startEnemyThreads(SharedState *state, pthread_t threads[], int count)
{
    if (count <= 0 || count > MAX_ENEMIES)
    {
        return;
    }

    g_enemyThreadArgs = new EnemyThreadArg[count];

    for (int i = 0; i < count; i++)
    {
        g_enemyThreadArgs[i].state = state;
        g_enemyThreadArgs[i].enemyId = i;
        pthread_create(&threads[i], NULL, enemyThreadFunction,
                       &g_enemyThreadArgs[i]);
    }
}

void joinEnemyThreads(int count, pthread_t threads[])
{
    if (count <= 0 || count > MAX_ENEMIES)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    delete[] g_enemyThreadArgs;
    g_enemyThreadArgs = nullptr;
}