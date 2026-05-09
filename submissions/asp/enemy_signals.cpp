

#include <iostream>
#include <csignal>
#include <pthread.h>

#include "enemy_signals.h"


static SharedState *g_signalState = nullptr;


static void handleSigusr1(int )
{
    if (!g_signalState)
        return;

    
    int targetId = g_signalState->stunTargetId;

    if (targetId >= 0 && targetId < MAX_ENEMIES)
    {
        g_signalState->enemies[targetId].stunned = 1;
        std::cout << "[ASP] SIGUSR1: Enemy " << targetId
                  << " marked stunned." << std::endl;
    }
}

void setupEnemySignalHandlers(SharedState *state)
{
    g_signalState = state;

    
    struct sigaction sa;
    sa.sa_handler = handleSigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, nullptr);

    
    signal(SIGTERM, SIG_DFL);

    
    sigset_t blockSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &blockSet, nullptr);
}