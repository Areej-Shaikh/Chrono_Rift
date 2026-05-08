#ifndef ENEMY_THREADS_H
#define ENEMY_THREADS_H

#include <pthread.h>
#include "shared_state.h"

void startEnemyThreads(SharedState *state, pthread_t threads[], int count);
void joinEnemyThreads(int count, pthread_t threads[]);
void notifyEnemyDeath(SharedState *state, int enemyId);
void cleanupEnemyThreads(SharedState *state, pthread_t threads[], int count);

#endif
