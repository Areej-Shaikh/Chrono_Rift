#ifndef ENEMY_SIGNALS_H
#define ENEMY_SIGNALS_H

#include <signal.h>
#include "shared_state.h"

// Sets up signal handlers for the ASP process.
// Must be called after shared memory is attached.
// state is stored globally so the SIGUSR1 handler can reach it.
void setupEnemySignalHandlers(SharedState *state);

#endif