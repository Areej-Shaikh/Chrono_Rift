#ifndef ENEMY_SIGNALS_H
#define ENEMY_SIGNALS_H

#include <signal.h>
#include <pthread.h>

extern volatile sig_atomic_t aspStunned;
extern pthread_mutex_t stunMutex;
extern pthread_cond_t stunCond;

void setupEnemySignalHandlers();

#endif
