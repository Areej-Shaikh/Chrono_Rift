

#include <iostream>
#include <csignal>
#include <pthread.h>
#include <unistd.h>
#include "enemy_signals.h"


static SharedState *g_signalState = nullptr;

static void handleSigusr1(int)
{
    std::cout << "[ASP] SIGUSR1 received. Enemy stunned for 3 seconds." << std::endl;
    sleep(3);
    std::cout << "[ASP] Stun finished. Enemy logic resumed." << std::endl;
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