#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cerrno>

#include "shared_memory.h"
#include "shared_state.h"

using namespace std;

// ─────────────────────────────────────────────
// Roll number: 0620
//   Full number used as seed  : 620
//   Last 2 digits             : 20   → enemy base HP offset
//   Last digit                : 0    → player damage offset  (0 + 10 = 10)
//   Second-last digit         : 2    → enemy  damage offset  (2 + 10 = 12)
// ─────────────────────────────────────────────
const int ROLL_NUMBER       = 620;
const int ROLL_LAST2        = 20;
const int ROLL_LAST1        = 0;
const int ROLL_SECOND_LAST  = 2;

// ─────────────────────────────────────────────
// Global pointer so signal handlers can reach state
// ─────────────────────────────────────────────
static SharedState* g_state = nullptr;
static pid_t        g_aspPid = -1;   // PID of Automated Strategic Process (to be set later)

// ─────────────────────────────────────────────
// SIGTERM handler – player chose to quit
// ─────────────────────────────────────────────
void handleSigterm(int sig) {
    (void)sig;
    if (g_state != nullptr) {
        sem_wait(&g_state->stateLock);
        g_state->gameStatus = GAME_QUIT;
        sem_post(&g_state->stateLock);
    }
    cout << "[Arbiter] SIGTERM received. Game set to QUIT." << endl;
}

// ─────────────────────────────────────────────
// SIGALRM handler – used for Ultimate Ability 10-second window expiry
// (stub: full implementation comes with signals section)
// ─────────────────────────────────────────────
void handleSigalrm(int sig) {
    (void)sig;
    // When Ultimate Ability timer fires, resume ASP
    if (g_aspPid > 0) {
        kill(g_aspPid, SIGCONT);
        cout << "[Arbiter] SIGALRM: Ultimate Ability window expired. ASP resumed." << endl;
    }
}

// ─────────────────────────────────────────────
// Helpers: random in range [lo, hi]
// ─────────────────────────────────────────────
static int randRange(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}

// ─────────────────────────────────────────────
// Initialize all entities with roll-number-based stats
// Called after HIP signals partySizeSelected
// ─────────────────────────────────────────────
void initEntities(SharedState* state) {
    srand(ROLL_NUMBER);

    int playerCount = state->partySize;
    state->playerCount = playerCount;

    int playerSpeed = 100 / playerCount;

    for (int i = 0; i < playerCount; i++) {
        int hp = ROLL_NUMBER + randRange(100, 1000);

        state->players[i].id      = i;
        state->players[i].maxHp   = hp;
        state->players[i].hp      = hp;
        state->players[i].damage  = ROLL_LAST1 + 10;   // 10
        state->players[i].speed   = playerSpeed;
        state->players[i].stamina = 0;
        state->players[i].alive   = 1;
    }

    // Enemy count: random 2–9
    int enemyCount = randRange(2, 9);
    state->enemyCount = enemyCount;

    for (int i = 0; i < enemyCount; i++) {
        int hp = ROLL_LAST2 + randRange(50, 200);    // 20 + rand(50,200)

        state->enemies[i].id      = i;
        state->enemies[i].maxHp   = hp;
        state->enemies[i].hp      = hp;
        state->enemies[i].damage  = ROLL_SECOND_LAST + 10; // 12
        state->enemies[i].speed   = randRange(10, 30);
        state->enemies[i].stamina = 0;
        state->enemies[i].alive   = 1;
    }

    state->currentTurnType = ENTITY_NONE;
    state->currentTurnId   = -1;
    state->enemiesKilled   = 0;
    state->gameStatus      = GAME_RUNNING;
    state->gameInitialized = 1;

    cout << "[Arbiter] Entities initialized." << endl;
    cout << "[Arbiter] Players: " << playerCount
         << "  Enemies: " << enemyCount
         << "  Player speed: " << playerSpeed << endl;

    for (int i = 0; i < playerCount; i++) {
        cout << "  Player " << i
             << "  HP=" << state->players[i].hp
             << "  DMG=" << state->players[i].damage
             << "  SPD=" << state->players[i].speed << endl;
    }
    for (int i = 0; i < enemyCount; i++) {
        cout << "  Enemy " << i
             << "  HP=" << state->enemies[i].hp
             << "  DMG=" << state->enemies[i].damage
             << "  SPD=" << state->enemies[i].speed << endl;
    }
}

// ─────────────────────────────────────────────
// Check win / lose conditions
// Returns GAME_WIN, GAME_LOSE, or GAME_RUNNING
// Must be called with stateLock held
// ─────────────────────────────────────────────
int checkGameOver(SharedState* state) {
    // Win: 10 enemies killed total
    if (state->enemiesKilled >= 10) {
        return GAME_WIN;
    }

    // Lose: all players dead
    int anyAlive = 0;
    for (int i = 0; i < state->playerCount; i++) {
        if (state->players[i].alive == 1) {
            anyAlive = 1;
            break;
        }
    }
    if (anyAlive == 0) {
        return GAME_LOSE;
    }

    return GAME_RUNNING;
}
void addActionLog(SharedState* state, const char* text) {
    for (int i = 4; i > 0; i--) {
        strcpy(state->actionLog[i], state->actionLog[i - 1]);
    }

    strncpy(state->actionLog[0], text, 99);
    state->actionLog[0][99] = '\0';

    if (state->actionLogCount < 5) {
        state->actionLogCount++;
    }
}
// ─────────────────────────────────────────────
// Process an action request that arrived via actionReady semaphore
// Must be called WITHOUT stateLock held (we acquire it inside)
// ─────────────────────────────────────────────
void processAction(SharedState* state) {
    sem_wait(&state->stateLock);

    ActionRequest req = state->request;
    state->request.ready = 0;   // consume

    if (req.entityType == ENTITY_PLAYER) {
        int pid = req.entityId;

        if (req.actionType == ACTION_STRIKE) {
            int tid = req.targetId;

            if (tid >= 0 && tid < state->enemyCount && state->enemies[tid].alive == 1) {
                int dmg = state->players[pid].damage;
                state->enemies[tid].hp -= dmg;
char logText[100];
sprintf(logText, "Player %d struck Enemy %d for %d damage", pid, tid, dmg);
addActionLog(state, logText);
                cout << "[Arbiter] Player " << pid
                     << " struck Enemy " << tid
                     << " for " << dmg << " dmg."
                     << " Enemy HP now: " << state->enemies[tid].hp << endl;

                if (state->enemies[tid].hp <= 0) {
                    state->enemies[tid].hp    = 0;
                    state->enemies[tid].alive = 0;
                    state->enemies[tid].stamina = 0;
                    state->enemiesKilled++;

                    cout << "[Arbiter] Enemy " << tid << " defeated! Total kills: "
                         << state->enemiesKilled << endl;
                }
            }
        }
        else if (req.actionType == ACTION_SKIP) {
            // Skip: stamina set to 50 (handled below in stamina depletion)
            char logText[100];
sprintf(logText, "Player %d skipped turn", pid);
addActionLog(state, logText);
            cout << "[Arbiter] Player " << pid << " skipped." << endl;
        }

        // Deplete acting player's stamina
        if (req.actionType == ACTION_SKIP) {
            state->players[pid].stamina = 50;
        }
        else {
            state->players[pid].stamina = 0;
        }
    }
    else if (req.entityType == ENTITY_ENEMY) {
        int eid = req.entityId;

        if (req.actionType == ACTION_STRIKE) {
            int tid = req.targetId;

            if (tid >= 0 && tid < state->playerCount && state->players[tid].alive == 1) {
                int dmg = state->enemies[eid].damage;
                state->players[tid].hp -= dmg;
state->lastNpcActionEnemyId = eid;
state->lastNpcActionType = ACTION_STRIKE;
state->lastNpcTargetPlayerId = tid;

char logText[100];
sprintf(logText, "Enemy %d struck Player %d for %d damage", eid, tid, dmg);
addActionLog(state, logText);
                cout << "[Arbiter] Enemy " << eid
                     << " struck Player " << tid
                     << " for " << dmg << " dmg."
                     << " Player HP now: " << state->players[tid].hp << endl;

                if (state->players[tid].hp <= 0) {
                    state->players[tid].hp    = 0;
                    state->players[tid].alive = 0;
                    state->players[tid].stamina = 0;

                    cout << "[Arbiter] Player " << tid << " defeated!" << endl;
                }
            }
        }
        else {
            state->lastNpcActionEnemyId = eid;
state->lastNpcActionType = ACTION_SKIP;
state->lastNpcTargetPlayerId = -1;

char logText[100];
sprintf(logText, "Enemy %d skipped turn", eid);
addActionLog(state, logText);
            cout << "[Arbiter] Enemy " << eid << " skipped." << endl;
        }

        // Deplete acting enemy's stamina
        if (req.actionType == ACTION_SKIP) {
            state->enemies[eid].stamina = 50;
        }
        else {
            state->enemies[eid].stamina = 0;
        }
    }

    // Check win/lose after processing
    int status = checkGameOver(state);
    if (status != GAME_RUNNING) {
        state->gameStatus = status;

        if (status == GAME_WIN) {
            cout << "[Arbiter] *** GAME WIN *** Players killed 10 enemies!" << endl;
        }
        else if (status == GAME_LOSE) {
            cout << "[Arbiter] *** GAME LOSE *** All players are dead." << endl;
        }
    }

    // Reset turn
    state->currentTurnType = ENTITY_NONE;
    state->currentTurnId   = -1;

    sem_post(&state->stateLock);

    // Notify acting entity that their action was processed
    sem_post(&state->actionDone);
}

// ─────────────────────────────────────────────
// Find which entity has the highest stamina and
// has reached its max. Returns true if found.
// Must be called with stateLock held.
// Fills outType and outId.
// ─────────────────────────────────────────────
int findNextActor(SharedState* state, int& outType, int& outId) {
    // We look for the entity whose stamina >= its max AND is highest
    // Ties broken by entity type (player first) then by id

    int bestType  = ENTITY_NONE;
    int bestId    = -1;
    int bestStam  = -1;

    for (int i = 0; i < state->playerCount; i++) {
        if (state->players[i].alive == 0) continue;
        if (state->players[i].stamina >= PLAYER_MAX_STAMINA) {
            if (state->players[i].stamina > bestStam) {
                bestStam = state->players[i].stamina;
                bestType = ENTITY_PLAYER;
                bestId   = i;
            }
        }
    }

    for (int i = 0; i < state->enemyCount; i++) {
        if (state->enemies[i].alive == 0) continue;
        if (state->enemies[i].stamina >= ENEMY_MAX_STAMINA) {
            if (state->enemies[i].stamina > bestStam) {
                bestStam = state->enemies[i].stamina;
                bestType = ENTITY_ENEMY;
                bestId   = i;
            }
        }
    }

    if (bestType != ENTITY_NONE) {
        outType = bestType;
        outId   = bestId;
        return 1;
    }

    return 0;
}

// ─────────────────────────────────────────────
// Tick stamina for all alive entities by their speed
// Must be called with stateLock held
// ─────────────────────────────────────────────
void tickStamina(SharedState* state) {
    for (int i = 0; i < state->playerCount; i++) {
        if (state->players[i].alive == 0) continue;
        state->players[i].stamina += state->players[i].speed;

        if (state->players[i].stamina > PLAYER_MAX_STAMINA) {
            state->players[i].stamina = PLAYER_MAX_STAMINA;
        }
    }

    for (int i = 0; i < state->enemyCount; i++) {
        if (state->enemies[i].alive == 0) continue;
        state->enemies[i].stamina += state->enemies[i].speed;

        if (state->enemies[i].stamina > ENEMY_MAX_STAMINA) {
            state->enemies[i].stamina = ENEMY_MAX_STAMINA;
        }
    }
}

// ─────────────────────────────────────────────
// Wait for an action on the actionReady semaphore
// with a timeout of timeoutSecs seconds.
// Returns 1 if action arrived, 0 if timed out.
// ─────────────────────────────────────────────
int waitForActionWithTimeout(SharedState* state, int timeoutSecs) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutSecs;

    int ret = sem_timedwait(&state->actionReady, &ts);

    if (ret == 0) {
        return 1;  // got action
    }

    if (errno == ETIMEDOUT) {
        return 0;  // timed out
    }

    // Other error (e.g. interrupted) — treat as timeout
    return 0;
}

// ─────────────────────────────────────────────
// Handle an enemy's turn
// Signals ASP (via currentTurnType/Id) and waits 3 seconds
// If no response, auto-skip
// ─────────────────────────────────────────────
void handleEnemyTurn(SharedState* state, int enemyId) {
    cout << "[Arbiter] Enemy " << enemyId << "'s turn." << endl;

    // Set turn so ASP knows who to move
    sem_wait(&state->stateLock);
    state->currentTurnType = ENTITY_ENEMY;
    state->currentTurnId   = enemyId;
    sem_post(&state->stateLock);

    // Wait up to 3 seconds for ASP to submit an action
    int got = waitForActionWithTimeout(state, 3);

    if (got == 0) {
        // Timeout: auto-skip
        cout << "[Arbiter] Enemy " << enemyId
             << " timed out. Auto-skip applied." << endl;

        sem_wait(&state->stateLock);
        state->request.ready      = 1;
        state->request.entityType = ENTITY_ENEMY;
        state->request.entityId   = enemyId;
        state->request.actionType = ACTION_SKIP;
        state->request.targetType = ENTITY_NONE;
        state->request.targetId   = -1;
        sem_post(&state->stateLock);
    }

    processAction(state);
}

// ─────────────────────────────────────────────
// Handle a player's turn
// Sets currentTurn so HIP knows who to prompt,
// then waits on actionReady (no timeout for players)
// ─────────────────────────────────────────────
void handlePlayerTurn(SharedState* state, int playerId) {
    cout << "[Arbiter] Player " << playerId << "'s turn." << endl;

    sem_wait(&state->stateLock);
    state->currentTurnType = ENTITY_PLAYER;
    state->currentTurnId   = playerId;
    sem_post(&state->stateLock);

    // Wait indefinitely for player input
    sem_wait(&state->actionReady);

    processAction(state);
}

// ─────────────────────────────────────────────
// Main scheduling loop
// ─────────────────────────────────────────────
void runGameLoop(SharedState* state) {
    cout << "[Arbiter] Game loop started." << endl;

    // Stamina tick interval: 1 second
    const int TICK_US = 1000000;

    while (true) {
        sem_wait(&state->stateLock);

        int status = state->gameStatus;

        if (status != GAME_RUNNING) {
            sem_post(&state->stateLock);
            break;
        }

        // Tick stamina for all entities
        tickStamina(state);

        // Find if anyone is ready to act
        int actorType = ENTITY_NONE;
        int actorId   = -1;
        int found     = findNextActor(state, actorType, actorId);

        sem_post(&state->stateLock);

        if (found == 1) {
            if (actorType == ENTITY_PLAYER) {
                handlePlayerTurn(state, actorId);
            }
            else if (actorType == ENTITY_ENEMY) {
                handleEnemyTurn(state, actorId);
            }
        }
        else {
            // No one ready yet, sleep for one tick
            usleep(TICK_US);
        }

        // Re-check game status after every action / tick
        sem_wait(&state->stateLock);
        status = state->gameStatus;
        sem_post(&state->stateLock);

        if (status != GAME_RUNNING) {
            break;
        }
    }

    cout << "[Arbiter] Game loop ended. Status: " << g_state->gameStatus << endl;
}

// ─────────────────────────────────────────────
// Graceful shutdown
// ─────────────────────────────────────────────
void shutdownGame(SharedState* state) {
    cout << "[Arbiter] Shutting down..." << endl;

    // Wake up any threads blocked on actionDone so they can exit
    sem_post(&state->actionDone);
    sem_post(&state->actionReady);

    // If ASP is running, terminate it
    if (g_aspPid > 0) {
        kill(g_aspPid, SIGTERM);
        waitpid(g_aspPid, nullptr, 0);
        cout << "[Arbiter] ASP terminated." << endl;
    }

    sleep(1); // Give HIP time to read final state

    destroySharedMemory();
    cout << "[Arbiter] Shared memory destroyed. Goodbye." << endl;
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    cout << "[Arbiter] Starting..." << endl;

    // Register signal handlers
    signal(SIGTERM, handleSigterm);
    signal(SIGALRM, handleSigalrm);

    // Create and initialize shared memory
    SharedState* state = createSharedMemory();

    if (state == nullptr) {
        cerr << "[Arbiter] Failed to create shared memory." << endl;
        return 1;
    }

    g_state = state;

    // Initialize semaphores and zero the state
    sem_init(&state->stateLock,  1, 1);
    sem_init(&state->actionReady, 1, 0);
    sem_init(&state->actionDone,  1, 0);

    memset(&state->inputBuffer, 0, sizeof(InputBuffer));
    state->inputBuffer.playerId   = -1;
    state->inputBuffer.actionType = ACTION_NONE;
    state->inputBuffer.targetType = ENTITY_NONE;
    state->inputBuffer.targetId   = -1;

    state->request.ready      = 0;
    state->partySizeSelected  = 0;
    state->partySize          = 0;
    state->gameInitialized    = 0;
    state->gameStatus         = GAME_RUNNING;
    state->enemiesKilled      = 0;
    state->currentTurnType    = ENTITY_NONE;
    state->currentTurnId      = -1;
for (int i = 0; i < MAX_ENEMIES; i++) {
    state->npcThreadAlive[i] = 0;
}

state->lastNpcActionEnemyId = -1;
state->lastNpcActionType = ACTION_NONE;
state->lastNpcTargetPlayerId = -1;
state->actionLogCount = 0;

for (int i = 0; i < 5; i++) {
    state->actionLog[i][0] = '\0';
}
    cout << "[Arbiter] Shared memory created. Waiting for HIP to select party size..." << endl;

    // ── Wait for HIP to set party size ──────────────────────────────
    while (state->partySizeSelected == 0) {
        usleep(100000);

        // Check if game was quit before even starting
        if (state->gameStatus == GAME_QUIT) {
            shutdownGame(state);
            return 0;
        }
    }

    cout << "[Arbiter] Party size selected: " << state->partySize << endl;

    // ── Initialize entities with roll-number-based stats ────────────
    initEntities(state);
g_aspPid = fork();

if (g_aspPid < 0) {
    cerr << "[Arbiter] Failed to fork ASP." << endl;
    shutdownGame(state);
    return 1;
}

if (g_aspPid == 0) {
    execl("./bin/asp", "./bin/asp", NULL);

    cerr << "[ASP Child] execl failed." << endl;
    exit(1);
}

cout << "[Arbiter] ASP forked with PID " << g_aspPid << endl;
    // ── Run the main scheduling loop ─────────────────────────────────
    runGameLoop(state);

    // ── Shutdown ─────────────────────────────────────────────────────
    shutdownGame(state);

    return 0;
}