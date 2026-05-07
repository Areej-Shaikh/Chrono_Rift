#include <iostream>
#include <signal.h>
#include <pthread.h>

#include "enemy_signals.h"

volatile sig_atomic_t aspStunned = 0;
pthread_mutex_t stunMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stunCond = PTHREAD_COND_INITIALIZER;
static sigset_t signalSet;

static void *signalThreadFunction(void *arg)
{
    (void)arg;
    int sig;

    while (true)
    {
        sigwait(&signalSet, &sig);

        if (sig == SIGUSR1)
        {
            aspStunned = 1;
            std::cout << "[ASP] STUN received. Pausing enemies for 3 seconds." << std::endl;
            alarm(3);
        }
        else if (sig == SIGALRM)
        {
            aspStunned = 0;
            std::cout << "[ASP] STUN ended. Enemies resumed." << std::endl;
            pthread_mutex_lock(&stunMutex);
            pthread_cond_broadcast(&stunCond);
            pthread_mutex_unlock(&stunMutex);
        }
    }

    return nullptr;
}

void setupEnemySignalHandlers()
{
    sigemptyset(&signalSet);
    sigaddset(&signalSet, SIGUSR1);
    sigaddset(&signalSet, SIGALRM);

    pthread_sigmask(SIG_BLOCK, &signalSet, NULL);

    pthread_t signalThread;
    pthread_create(&signalThread, NULL, signalThreadFunction, NULL);
    pthread_detach(signalThread);
}
