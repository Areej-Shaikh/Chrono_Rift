#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "shared_memory.h"
#include "enemy_threads.h"
#include "enemy_signals.h"

int runNpcProcess()
{
    srand(time(NULL) ^ getpid());

    SharedState *state = attachSharedMemory();

    if (state == NULL)
    {
        std::cout << "[ASP] Failed to attach shared memory." << std::endl;
        return 1;
    }

    std::cout << "[ASP] Attached to shared memory." << std::endl;

    sem_wait(&state->stateLock);
    state->aspPid = getpid();
    sem_post(&state->stateLock);

    while (state->gameInitialized == 0)
    {
        usleep(100000);
    }

    setupEnemySignalHandlers(state);

    sem_wait(&state->stateLock);
    int enemyCount = state->enemyCount;
    sem_post(&state->stateLock);

    pthread_t enemyThreads[MAX_ENEMIES];
    startEnemyThreads(state, enemyThreads, enemyCount);
    joinEnemyThreads(enemyCount, enemyThreads);

    detachSharedMemory(state);

    std::cout << "[ASP] Exiting." << std::endl;
    return 0;
}