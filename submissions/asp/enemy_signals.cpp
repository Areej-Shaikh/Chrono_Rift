/*
 * enemy_signals.cpp  —  Signal handling for the ASP process
 *
 * SIGUSR1 — Stun mechanic (spec Section 5):
 *   The Arbiter writes state->stunTargetId BEFORE sending SIGUSR1.
 *   The handler reads stunTargetId and sets state->enemies[id].stunned = 1.
 *   The enemy thread detects this at the top
 *   of its turn and sleeps exactly 3 seconds.
 *
 *   Stun is PER-ENEMY, not process-wide. Only the target enemy pauses.
 *   Other enemy threads continue polling normally.
 *
 * SIGALRM is not used by the ASP — the Arbiter handles its own SIGALRM
 * for the Ultimate Ability timer. Blocking it here prevents interference.
 *
 * SIGSTOP / SIGCONT — Ultimate Ability (spec Section 8):
 *   The Arbiter sends SIGSTOP to freeze the entire ASP process for 10s,
 *   then SIGCONT to resume. These are OS-level and cannot be caught.
 *   No handling needed — the kernel manages them automatically.
 */

#include <iostream>
#include <csignal>
#include <pthread.h>

#include "enemy_signals.h"

// Global shared state pointer — set once in setupEnemySignalHandlers()
static SharedState *g_signalState = nullptr;

// ─────────────────────────────────────────────
// SIGUSR1 handler — per-enemy stun
// Sets stunned flag for the exact enemy chosen by the Arbiter.
// Must not call sem_wait (signal handlers must not block).
// ─────────────────────────────────────────────
static void handleSigusr1(int /*sig*/)
{
    if (!g_signalState)
        return;

    // Arbiter writes stunTargetId before sending SIGUSR1.
    // Do not use currentTurnId here: during a player attack, currentTurnId is
    // the player id, not the enemy being hit. Reading an int is atomic here.
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

    // Install SIGUSR1 handler for per-enemy stun
    struct sigaction sa;
    sa.sa_handler = handleSigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, nullptr);

    // SIGTERM — let the process exit naturally (threads will notice game status)
    signal(SIGTERM, SIG_DFL);

    // SIGALRM — block it in ASP; only the Arbiter uses SIGALRM
    sigset_t blockSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &blockSet, nullptr);
}