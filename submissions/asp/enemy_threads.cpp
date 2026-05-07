#include <iostream>
#include <unistd.h>
#include <pthread.h>

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

    while (true)
    {
        pthread_mutex_lock(&stunMutex);
        while (aspStunned == 1)
        {
            pthread_cond_wait(&stunCond, &stunMutex);
        }
        pthread_mutex_unlock(&stunMutex);

        sem_wait(&state->stateLock);

        if (aspStunned == 1)
        {
            sem_post(&state->stateLock);
            continue;
        }

        int status = state->gameStatus;
        int currentType = state->currentTurnType;
        int currentId = state->currentTurnId;
        int alive = state->enemies[enemyId].alive;

        sem_post(&state->stateLock);

        if (status != GAME_RUNNING)
        {
            break;
        }

        if (alive == 1 && currentType == ENTITY_ENEMY && currentId == enemyId)
        {
            submitEnemyMove(enemyId, state);

            while (true)
            {
                sem_wait(&state->stateLock);

                int stillMyTurn = (state->currentTurnType == ENTITY_ENEMY && state->currentTurnId == enemyId);
                int gameRunning = (state->gameStatus == GAME_RUNNING);

                sem_post(&state->stateLock);

                if (stillMyTurn == 0 || gameRunning == 0)
                {
                    break;
                }

                usleep(50000);
            }
        }

        usleep(50000);
    }

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
        pthread_create(&threads[i], NULL, enemyThreadFunction, &g_enemyThreadArgs[i]);
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
