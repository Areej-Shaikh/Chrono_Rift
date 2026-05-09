

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


static void enemyThreadCleanupHandler(void *arg)
{
    EnemyThreadArg *threadArg = (EnemyThreadArg *)arg;
    SharedState    *state     = threadArg->state;
    int             enemyId   = threadArg->enemyId;

    
    pthread_mutex_lock(&state->threadCleanupMutex);
    state->npcThreadAlive[enemyId] = 0;
    cout << "[ASP] Enemy thread " << enemyId
              << " cleanup: marked dead." << endl;
    pthread_mutex_unlock(&state->threadCleanupMutex);
}


static void *enemyThreadFunction(void *arg)
{
    EnemyThreadArg *threadArg = (EnemyThreadArg *)arg;
    SharedState    *state     = threadArg->state;
    int             enemyId   = threadArg->enemyId;

    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);

    
    pthread_cleanup_push(enemyThreadCleanupHandler, arg);

    
    sem_wait(&state->stateLock);
    state->npcThreadAlive[enemyId] = 1;
    sem_post(&state->stateLock);

    while (true)
    {
        
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

        
        sem_wait(&state->stateLock);
        int myTurn = (state->currentTurnType == ENTITY_ENEMY &&
                      state->currentTurnId == enemyId);
        sem_post(&state->stateLock);

        if (myTurn == 0)
        {
            
            
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 50000000; 
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
        
      
sem_wait(&state->stateLock);

if (state->currentTurnType == ENTITY_ENEMY &&
    state->currentTurnId == enemyId &&
    state->request.ready == 0) {

    sem_post(&state->stateLock);

    submitEnemyMove(enemyId, state);

    
    sem_wait(&state->actionDone);

    
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

    
    pthread_cleanup_pop(1);
    return nullptr;
}


void startEnemyThreads(SharedState *state, pthread_t threads[], int count)
{
    if (count <= 0 || count > MAX_ENEMIES)
        return;

    
    g_enemyThreadArgs = new EnemyThreadArg[count];

    for (int i = 0; i < count; i++)
    {
        g_enemyThreadArgs[i].state   = state;
        g_enemyThreadArgs[i].enemyId = i;
        pthread_create(&threads[i], nullptr, enemyThreadFunction,
                       &g_enemyThreadArgs[i]);
    }
}


void joinEnemyThreads(int count, pthread_t threads[])
{
    if (count <= 0 || count > MAX_ENEMIES)
        return;

    for (int i = 0; i < count; i++)
        pthread_join(threads[i], nullptr);

    delete[] g_enemyThreadArgs;
    g_enemyThreadArgs = nullptr;
}


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


void cleanupEnemyThreads(SharedState *state, pthread_t threads[], int count)
{
    if (count <= 0 || count > MAX_ENEMIES || state == nullptr)
        return;

    cout << "[ASP] cleanupEnemyThreads: cancelling " << count
              << " threads." << endl;

    for (int i = 0; i < count; i++)
    {
        
        
        int ret = pthread_cancel(threads[i]);
        if (ret == 0)
            cout << "[ASP] Cancelled thread for enemy " << i << endl;
        else
            cout << "[ASP] Enemy " << i
                      << " thread already finished." << endl;
    }

    
    pthread_mutex_lock(&state->threadCleanupMutex);
    pthread_cond_broadcast(&state->threadCleanupCond);
    pthread_mutex_unlock(&state->threadCleanupMutex);

    cout << "[ASP] cleanupEnemyThreads: cancellation requests sent."
              << endl;
}