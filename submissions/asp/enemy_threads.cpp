/*
 * enemy_threads.cpp  —  Per-NPC pthread logic
 *
 * Key design decisions:
 *
 * 1. pthread_cond_timedwait requires a pthread_mutex_t, NOT a sem_t.
 *    SharedState now has a dedicated threadCleanupMutex alongside the
 *    existing stateLock semaphore. The mutex is used ONLY with the
 *    condition variable — all other shared state is still guarded by
 *    sem_wait(&state->stateLock).
 *
 * 2. Cleanup handler is safe w.r.t. semaphores.
 *    The handler does NOT call sem_wait. If a thread is cancelled while
 *    it holds stateLock (e.g. inside sem_wait), grabbing the semaphore
 *    again in the handler would deadlock. Instead the handler only uses
 *    the dedicated threadCleanupMutex, which the thread never holds
 *    while sleeping or blocking.
 *
 * 3. pthread_setcanceltype is set on each enemy thread itself, not in
 *    the caller. cleanupEnemyThreads() just fires pthread_cancel and
 *    then broadcasts the condition to wake any sleeping threads.
 *
 * 4. notifyEnemyDeath() is called from npc_process.cpp after the Arbiter
 *    marks an enemy dead, so threads wake up within 50 ms instead of
 *    waiting out their full poll interval.
 */

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <cstring>

#include "enemy_threads.h"
#include "enemy_actions.h"
#include "enemy_signals.h"
using namespace std;
struct EnemyThreadArg
{
    SharedState *state;
    int          enemyId;
};

static EnemyThreadArg *g_enemyThreadArgs = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup handler — registered with pthread_cleanup_push.
// Runs when the thread exits normally (cleanup_pop with execute=1)
// OR when pthread_cancel() is delivered.
//
// IMPORTANT: must NOT call sem_wait(stateLock) here. A thread can be
// cancelled while stateLock is already held (e.g. between sem_wait and
// sem_post). Re-entering sem_wait in that case would deadlock.
// We use the dedicated threadCleanupMutex instead, which the thread
// never holds while blocking on semaphores.
// ─────────────────────────────────────────────────────────────────────────────
static void enemyThreadCleanupHandler(void *arg)
{
    EnemyThreadArg *threadArg = (EnemyThreadArg *)arg;
    SharedState    *state     = threadArg->state;
    int             enemyId   = threadArg->enemyId;

    // Use threadCleanupMutex (not stateLock) to safely write npcThreadAlive.
    pthread_mutex_lock(&state->threadCleanupMutex);
    state->npcThreadAlive[enemyId] = 0;
    cout << "[ASP] Enemy thread " << enemyId
              << " cleanup: marked dead." << endl;
    pthread_mutex_unlock(&state->threadCleanupMutex);
}

// ─────────────────────────────────────────────────────────────────────────────
// enemyThreadFunction
// ─────────────────────────────────────────────────────────────────────────────
static void *enemyThreadFunction(void *arg)
{
    EnemyThreadArg *threadArg = (EnemyThreadArg *)arg;
    SharedState    *state     = threadArg->state;
    int             enemyId   = threadArg->enemyId;

    // Allow pthread_cancel() to take effect at cancellation points.
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);

    // Register cleanup handler (runs on exit or cancellation).
    pthread_cleanup_push(enemyThreadCleanupHandler, arg);

    // Mark alive using stateLock (normal shared-state path).
    sem_wait(&state->stateLock);
    state->npcThreadAlive[enemyId] = 1;
    sem_post(&state->stateLock);

    while (true)
    {
        // ── Check game status and own liveness ────────────────────────────
        sem_wait(&state->stateLock);
        int status = state->gameStatus;
        int alive  = state->enemies[enemyId].alive;
        sem_post(&state->stateLock);

        if (status != GAME_RUNNING || alive == 0)
        {
            cout << "[ASP] Enemy " << enemyId
                      << " exiting (status=" << status
                      << ", alive=" << alive << ")." << endl;
            break;
        }

        // ── Wait for our turn ─────────────────────────────────────────────
        sem_wait(&state->stateLock);
        int myTurn = (state->currentTurnType == ENTITY_ENEMY &&
                      state->currentTurnId == enemyId);
        sem_post(&state->stateLock);

        if (myTurn == 0)
        {
            // Block on the condition variable with a 50 ms timeout.
            // This wakes up immediately if notifyEnemyDeath() broadcasts,
            // or after 50 ms at the latest for a normal turn-check cycle.
            // We use threadCleanupMutex (not stateLock) because
            // pthread_cond_timedwait requires a pthread_mutex_t.
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 50000000; // 50 ms
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_sec  += 1;
                ts.tv_nsec -= 1000000000;
            }

            pthread_mutex_lock(&state->threadCleanupMutex);
            pthread_cond_timedwait(&state->threadCleanupCond,
                                   &state->threadCleanupMutex, &ts);
            pthread_mutex_unlock(&state->threadCleanupMutex);
            continue;
        }

        // ── Check per-enemy stun (spec Section 5) ─────────────────────────
        sem_wait(&state->stateLock);
        int wasStunned = state->enemies[enemyId].stunned;
        sem_post(&state->stateLock);
if (wasStunned == 1)
{
    cout << "[ASP] Enemy " << enemyId
         << " stunned. Pausing 3 seconds." << endl;

    struct timespec ts = {3, 0};
    nanosleep(&ts, nullptr);

    sem_wait(&state->stateLock);
    state->enemies[enemyId].stunned = 0;
    sem_post(&state->stateLock);

    cout << "[ASP] Enemy " << enemyId
         << " stun ended." << endl;

    continue;
}
        // ── Submit action ─────────────────────────────────────────────────
      // ── Submit action only once for this turn ─────────────────────────
sem_wait(&state->stateLock);

if (state->currentTurnType == ENTITY_ENEMY &&
    state->currentTurnId == enemyId &&
    state->request.ready == 0) {

    sem_post(&state->stateLock);

    submitEnemyMove(enemyId, state);

    // Wait until Arbiter processes this exact move
    sem_wait(&state->actionDone);

    // Wait until Arbiter moves turn away from this enemy
    while (true) {
        sem_wait(&state->stateLock);
        int stillMyTurn = (state->currentTurnType == ENTITY_ENEMY &&
                           state->currentTurnId == enemyId);
        sem_post(&state->stateLock);

        if (stillMyTurn == 0)
            break;

        usleep(10000);
    }
}
else {
    sem_post(&state->stateLock);
}
    }

    // Execute the cleanup handler (marks npcThreadAlive[enemyId] = 0).
    pthread_cleanup_pop(1);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// startEnemyThreads
// ─────────────────────────────────────────────────────────────────────────────
void startEnemyThreads(SharedState *state, pthread_t threads[], int count)
{
    if (count <= 0 || count > MAX_ENEMIES)
        return;

    // Initialize the mutex and condition variable that back notifyEnemyDeath.
    // pshared=0: only used within this process (ASP).


    g_enemyThreadArgs = new EnemyThreadArg[count];

    for (int i = 0; i < count; i++)
    {
        g_enemyThreadArgs[i].state   = state;
        g_enemyThreadArgs[i].enemyId = i;
        pthread_create(&threads[i], nullptr, enemyThreadFunction,
                       &g_enemyThreadArgs[i]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// joinEnemyThreads — wait for all threads to finish naturally
// ─────────────────────────────────────────────────────────────────────────────
void joinEnemyThreads(int count, pthread_t threads[])
{
    if (count <= 0 || count > MAX_ENEMIES)
        return;

    for (int i = 0; i < count; i++)
        pthread_join(threads[i], nullptr);

    delete[] g_enemyThreadArgs;
    g_enemyThreadArgs = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// notifyEnemyDeath — wake sleeping threads immediately when an enemy dies
//
// Called from npc_process.cpp (or wherever the death event is observed)
// so threads don't have to wait up to 50 ms for their poll to expire.
// ─────────────────────────────────────────────────────────────────────────────
void notifyEnemyDeath(SharedState *state, int enemyId)
{
    if (state == nullptr)
        return;

    cout << "[ASP] notifyEnemyDeath: broadcasting for enemy "
              << enemyId << "." << endl;

    pthread_mutex_lock(&state->threadCleanupMutex);
    pthread_cond_broadcast(&state->threadCleanupCond);
    pthread_mutex_unlock(&state->threadCleanupMutex);
}

// ─────────────────────────────────────────────────────────────────────────────
// cleanupEnemyThreads — cancel any threads still running at shutdown
//
// Call this BEFORE joinEnemyThreads() if a graceful exit is not possible
// (e.g. game ended while some enemy threads are blocked on actionDone).
// ─────────────────────────────────────────────────────────────────────────────
void cleanupEnemyThreads(SharedState *state, pthread_t threads[], int count)
{
    if (count <= 0 || count > MAX_ENEMIES || state == nullptr)
        return;

    cout << "[ASP] cleanupEnemyThreads: cancelling " << count
              << " threads." << endl;

    for (int i = 0; i < count; i++)
    {
        // pthread_cancel sends a cancellation request to threads[i].
        // Because the thread uses DEFERRED cancellation, it will only
        // act at the next cancellation point (pthread_cond_timedwait,
        // nanosleep, sem_wait, etc.) — the cleanup handler then runs.
        int ret = pthread_cancel(threads[i]);
        if (ret == 0)
            cout << "[ASP] Cancelled thread for enemy " << i << endl;
        else
            cout << "[ASP] Enemy " << i
                      << " thread already finished." << endl;
    }

    // Broadcast so any thread sleeping on the condition variable wakes
    // up and hits a cancellation point promptly.
    pthread_mutex_lock(&state->threadCleanupMutex);
    pthread_cond_broadcast(&state->threadCleanupCond);
    pthread_mutex_unlock(&state->threadCleanupMutex);

    cout << "[ASP] cleanupEnemyThreads: cancellation requests sent."
              << endl;
}