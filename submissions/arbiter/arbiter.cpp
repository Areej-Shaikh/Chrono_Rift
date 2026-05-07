#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <pthread.h>
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
const int ROLL_NUMBER = 620;
const int ROLL_LAST2 = 20;
const int ROLL_LAST1 = 0;
const int ROLL_SECOND_LAST = 2;

// ─────────────────────────────────────────────
// Global pointer so signal handlers can reach state
// ─────────────────────────────────────────────
static SharedState *g_state = nullptr;
static pid_t g_aspPid = -1; // PID of Automated Strategic Process
static pid_t g_hipPid = -1; // PID of Human Interfacing Process
void addActionLog(SharedState *state, const char *text);
static int g_staminaThreadRunning = 1;
// ─────────────────────────────────────────────
// SIGTERM handler – player chose to quit
// ─────────────────────────────────────────────
void handleSigterm(int sig)
{
    (void)sig;

    if (g_state != nullptr)
    {
        sem_wait(&g_state->stateLock);
        g_state->gameStatus = GAME_QUIT;
        sem_post(&g_state->stateLock);
    }

    cout << "[Arbiter] SIGTERM received. Game set to QUIT." << endl;
}
void handleSigalrm(int sig)
{
    (void)sig;

    if (g_state != nullptr)
    {
        sem_wait(&g_state->stateLock);
        g_state->ultimateActive = 0;
        sem_post(&g_state->stateLock);
    }

    if (g_aspPid > 0)
    {
        kill(g_aspPid, SIGCONT);
        cout << "[Arbiter] Ultimate ended after 10 seconds. ASP resumed." << endl;
    }
}
// ─────────────────────────────────────────────
// SIGALRM handler – used for Ultimate Ability 10-second window expiry
// (stub: full implementation comes with signals section)
// ─────────────────────────────────────────────

// ─────────────────────────────────────────────
// Helpers: random in range [lo, hi]
// ─────────────────────────────────────────────
static int randRange(int lo, int hi)
{
    return lo + rand() % (hi - lo + 1);
}
void copyName(char dest[], const char src[])
{
    int i = 0;

    while (src[i] != '\0')
    {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

Weapon createWeapon(const char name[], int slotSize, int damage, int isArtifact)
{
    Weapon w;
    copyName(w.name, name);
    w.slotSize = slotSize;
    w.damage = damage;
    w.isArtifact = isArtifact;
    return w;
}

void initializeInventory(PlayerInventory &inv)
{
    for (int i = 0; i < INVENTORY_SIZE; i++)
    {
        inv.slots[i] = -1;
    }

    for (int i = 0; i < MAX_WEAPONS; i++)
    {
        inv.weapons[i].startSlot = -1;
        inv.weapons[i].active = 0;
    }

    inv.weaponCount = 0;
    inv.storageCount = 0;
}

int findContiguousSpace(PlayerInventory &inv, int neededSlots)
{
    for (int i = 0; i <= INVENTORY_SIZE - neededSlots; i++)
    {
        int freeSpace = 1;

        for (int j = i; j < i + neededSlots; j++)
        {
            if (inv.slots[j] != -1)
            {
                freeSpace = 0;
                break;
            }
        }

        if (freeSpace == 1)
        {
            return i;
        }
    }

    return -1;
}

void removeWeaponToStorage(PlayerInventory &inv, int weaponIndex)
{
    if (weaponIndex < 0 || weaponIndex >= inv.weaponCount)
    {
        return;
    }

    if (inv.weapons[weaponIndex].active == 0)
    {
        return;
    }

    int start = inv.weapons[weaponIndex].startSlot;
    int size = inv.weapons[weaponIndex].weapon.slotSize;

    for (int i = start; i < start + size; i++)
    {
        inv.slots[i] = -1;
    }

    if (inv.storageCount < MAX_STORAGE)
    {
        inv.longTermStorage[inv.storageCount] = inv.weapons[weaponIndex].weapon;
        inv.storageCount++;
    }

    inv.weapons[weaponIndex].active = 0;
    inv.weapons[weaponIndex].startSlot = -1;
}

void swapOutEnoughWeapons(PlayerInventory &inv, int neededSlots)
{
    while (findContiguousSpace(inv, neededSlots) == -1)
    {
        int bestWeapon = -1;
        int smallestSize = 999;

        for (int i = 0; i < inv.weaponCount; i++)
        {
            if (inv.weapons[i].active == 1)
            {
                int size = inv.weapons[i].weapon.slotSize;

                if (size < smallestSize)
                {
                    smallestSize = size;
                    bestWeapon = i;
                }
            }
        }

        if (bestWeapon == -1)
        {
            break;
        }

        removeWeaponToStorage(inv, bestWeapon);
    }
}

int addWeaponToInventory(PlayerInventory &inv, Weapon weapon)
{
    if (weapon.slotSize > INVENTORY_SIZE)
    {
        return 0;
    }

    int start = findContiguousSpace(inv, weapon.slotSize);

    if (start == -1)
    {
        swapOutEnoughWeapons(inv, weapon.slotSize);
        start = findContiguousSpace(inv, weapon.slotSize);
    }

    if (start == -1 || inv.weaponCount >= MAX_WEAPONS)
    {
        return 0;
    }

    int index = inv.weaponCount;
    inv.weaponCount++;

    inv.weapons[index].weapon = weapon;
    inv.weapons[index].startSlot = start;
    inv.weapons[index].active = 1;

    for (int i = start; i < start + weapon.slotSize; i++)
    {
        inv.slots[i] = index;
    }

    return 1;
}

int swapInWeapon(PlayerInventory &inv, int storageIndex)
{
    if (storageIndex < 0 || storageIndex >= inv.storageCount)
    {
        return 0;
    }

    Weapon weapon = inv.longTermStorage[storageIndex];

    int added = addWeaponToInventory(inv, weapon);

    if (added == 0)
    {
        return 0;
    }

    for (int i = storageIndex; i < inv.storageCount - 1; i++)
    {
        inv.longTermStorage[i] = inv.longTermStorage[i + 1];
    }

    inv.storageCount--;

    return 1;
}
Weapon getRandomDroppedWeapon()
{
    int r = rand() % 8;

    if (r == 0)
    {
        return createWeapon("Solar Core", 10, 95, 1);
    }
    else if (r == 1)
    {
        return createWeapon("Lunar Blade", 10, 90, 1);
    }
    else if (r == 2)
    {
        return createWeapon("Iron Halberd", 7, 55, 0);
    }
    else if (r == 3)
    {
        return createWeapon("Venom Dagger", 4, 30, 0);
    }
    else if (r == 4)
    {
        return createWeapon("Thunderstaff", 6, 50, 0);
    }
    else if (r == 5)
    {
        return createWeapon("Obsidian Axe", 5, 45, 0);
    }
    else if (r == 6)
    {
        return createWeapon("Frostbow", 6, 48, 0);
    }

    return createWeapon("Splinter Stick", 2, 12, 0);
}

void handleWeaponDrop(SharedState *state, int playerId)
{
    int dropChance = rand() % 100;

    if (dropChance < 50)
    {
        Weapon dropped = getRandomDroppedWeapon();

        int added = addWeaponToInventory(state->players[playerId].inventory, dropped);

        char logText[120];

        if (added == 1)
        {
            sprintf(logText,
                    "DROP: %s picked up by Player %d",
                    dropped.name,
                    playerId);
        }
        else
        {
            sprintf(logText,
                    "DROP: %s dropped by Enemy, but Player %d's inventory was full.",
                    dropped.name,
                    playerId);
        }

        addActionLog(state, logText);

        cout << "[Arbiter] " << logText << endl;
    }
}
void spawnEnemyAt(SharedState *state, int index)
{
    int hp = ROLL_LAST2 + randRange(50, 200);

    state->enemies[index].id = index;
    state->enemies[index].maxHp = hp;
    state->enemies[index].hp = hp;
    state->enemies[index].damage = ROLL_SECOND_LAST + 10;
    state->enemies[index].speed = randRange(10, 30);
    state->enemies[index].stamina = 0;
    state->enemies[index].alive = 1;
}

// ─────────────────────────────────────────────
// Initialize all entities with roll-number-based stats
// Called after HIP signals partySizeSelected
// ─────────────────────────────────────────────
void initEntities(SharedState *state)
{
    srand(ROLL_NUMBER);

    int playerCount = state->partySize;
    state->playerCount = playerCount;

    int playerSpeed = 100 / playerCount;

    for (int i = 0; i < playerCount; i++)
    {
        int hp = ROLL_NUMBER + randRange(100, 1000);

        state->players[i].id = i;
        state->players[i].maxHp = hp;
        state->players[i].hp = hp;
        state->players[i].damage = ROLL_LAST1 + 10; // 10
        state->players[i].speed = playerSpeed;
        state->players[i].stamina = 0;
        state->players[i].alive = 1;
        initializeInventory(state->players[i].inventory);
        if (i == 0)
        {
            addWeaponToInventory(state->players[i].inventory, createWeapon("Solar Core", 10, 95, 1));
            addWeaponToInventory(state->players[i].inventory, createWeapon("Lunar Blade", 10, 90, 1));
        }
    }

    // Enemy count: random 2–9
    int enemyCount = randRange(2, 9);
    state->enemyCount = enemyCount;

    for (int i = 0; i < enemyCount; i++)
    {
        spawnEnemyAt(state, i);
    }

    state->currentTurnType = ENTITY_NONE;
    state->currentTurnId = -1;
    state->enemiesKilled = 0;
    state->gameStatus = GAME_RUNNING;
    state->gameInitialized = 1;

    cout << "[Arbiter] Entities initialized." << endl;
    cout << "[Arbiter] Players: " << playerCount
         << "  Enemies: " << enemyCount
         << "  Player speed: " << playerSpeed << endl;

    for (int i = 0; i < playerCount; i++)
    {
        cout << "  Player " << i
             << "  HP=" << state->players[i].hp
             << "  DMG=" << state->players[i].damage
             << "  SPD=" << state->players[i].speed << endl;
    }
    for (int i = 0; i < enemyCount; i++)
    {
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
int checkGameOver(SharedState *state)
{
    // Win: 10 enemies killed total
    if (state->enemiesKilled >= 10)
    {
        return GAME_WIN;
    }

    // Lose: all players dead
    int anyAlive = 0;
    for (int i = 0; i < state->playerCount; i++)
    {
        if (state->players[i].alive == 1)
        {
            anyAlive = 1;
            break;
        }
    }
    if (anyAlive == 0)
    {
        return GAME_LOSE;
    }

    return GAME_RUNNING;
}
void addActionLog(SharedState *state, const char *text)
{
    for (int i = 4; i > 0; i--)
    {
        strcpy(state->actionLog[i], state->actionLog[i - 1]);
    }

    strncpy(state->actionLog[0], text, 99);
    state->actionLog[0][99] = '\0';

    if (state->actionLogCount < 5)
    {
        state->actionLogCount++;
    }
}
// ─────────────────────────────────────────────
// Process an action request that arrived via actionReady semaphore
// Must be called WITHOUT stateLock held (we acquire it inside)
// ─────────────────────────────────────────────
int playerHasSolarAndLunar(SharedState *state, int pid)
{
    int hasSolar = 0;
    int hasLunar = 0;

    for (int i = 0; i < state->players[pid].inventory.weaponCount; i++)
    {
        if (state->players[pid].inventory.weapons[i].active == 1)
        {
            if (strcmp(state->players[pid].inventory.weapons[i].weapon.name, "Solar Core") == 0)
            {
                hasSolar = 1;
            }

            if (strcmp(state->players[pid].inventory.weapons[i].weapon.name, "Lunar Blade") == 0)
            {
                hasLunar = 1;
            }
        }
    }

    return hasSolar == 1 && hasLunar == 1;
}
void processAction(SharedState *state)
{
    sem_wait(&state->stateLock);

    ActionRequest req = state->request;
    state->request.ready = 0;

    if (req.entityType == ENTITY_PLAYER)
    {
        int pid = req.entityId;

        if (req.actionType == ACTION_STRIKE)
        {
            int tid = req.targetId;

            if (tid >= 0 && tid < state->enemyCount && state->enemies[tid].alive == 1)
            {
                int dmg = state->players[pid].damage;
                state->enemies[tid].hp -= dmg;
                kill(g_aspPid, SIGUSR1);
                addActionLog(state, "STUN: Strategic Process stunned for 3 seconds");

                char logText[100];
                sprintf(logText, "Player %d struck Enemy %d for %d damage", pid, tid, dmg);
                addActionLog(state, logText);

                cout << "[Arbiter] Player " << pid
                     << " struck Enemy " << tid
                     << " for " << dmg << " dmg."
                     << " Enemy HP now: " << state->enemies[tid].hp << endl;

                if (state->enemies[tid].hp <= 0)
                {
                    state->enemies[tid].hp = 0;
                    state->enemies[tid].alive = 0;
                    state->enemies[tid].stamina = 0;
                    state->enemiesKilled++;
                    handleWeaponDrop(state, pid);
                    cout << "[Arbiter] Enemy " << tid << " defeated! Total kills: "
                         << state->enemiesKilled << endl;

                    if (state->enemiesKilled < 10)
                    {
                        spawnEnemyAt(state, tid);
                        char spawnLog[100];
                        sprintf(spawnLog, "A new Enemy %d entered the rift", tid);
                        addActionLog(state, spawnLog);
                        cout << "[Arbiter] Enemy " << tid << " respawned for continued combat." << endl;
                    }
                }
            }
        }
        else if (req.actionType == ACTION_EXHAUST)
        {
            int tid = req.targetId;

            if (tid >= 0 && tid < state->enemyCount && state->enemies[tid].alive == 1)
            {
                int reduce = state->players[pid].damage;
                state->enemies[tid].stamina -= reduce;

                if (state->enemies[tid].stamina < 0)
                {
                    state->enemies[tid].stamina = 0;
                }

                char logText[100];
                sprintf(logText, "Player %d exhausted Enemy %d by %d stamina", pid, tid, reduce);
                addActionLog(state, logText);

                cout << "[Arbiter] Player " << pid
                     << " exhausted Enemy " << tid
                     << " by " << reduce << " stamina."
                     << " Enemy stamina now: " << state->enemies[tid].stamina << endl;
            }
        }
        else if (req.actionType == ACTION_HEAL)
        {
            int healAmount = state->players[pid].maxHp / 10;

            if (healAmount <= 0)
            {
                healAmount = 1;
            }

            state->players[pid].hp += healAmount;

            if (state->players[pid].hp > state->players[pid].maxHp)
            {
                state->players[pid].hp = state->players[pid].maxHp;
            }

            char logText[100];
            sprintf(logText, "Player %d healed by %d HP", pid, healAmount);
            addActionLog(state, logText);

            cout << "[Arbiter] Player " << pid
                 << " healed by " << healAmount
                 << " HP. Current HP: " << state->players[pid].hp << endl;
        }
        else if (req.actionType == ACTION_USE_WEAPON)
        {
            int tid = req.targetId;
            int weaponIndex = req.targetType;

            if (weaponIndex >= 0 &&
                weaponIndex < state->players[pid].inventory.weaponCount &&
                state->players[pid].inventory.weapons[weaponIndex].active == 1 &&
                tid >= 0 &&
                tid < state->enemyCount &&
                state->enemies[tid].alive == 1)
            {

                int dmg = state->players[pid].inventory.weapons[weaponIndex].weapon.damage;
                state->enemies[tid].hp -= dmg;

                char logText[100];
                sprintf(logText, "Player %d used %s on Enemy %d for %d damage",
                        pid,
                        state->players[pid].inventory.weapons[weaponIndex].weapon.name,
                        tid,
                        dmg);
                addActionLog(state, logText);

                if (state->enemies[tid].hp <= 0)
                {
                    state->enemies[tid].hp = 0;
                    state->enemies[tid].alive = 0;
                    state->enemies[tid].stamina = 0;
                    state->enemiesKilled++;
                    handleWeaponDrop(state, pid);
                    if (state->enemiesKilled < 10)
                    {
                        spawnEnemyAt(state, tid);
                        addActionLog(state, "A new enemy entered the rift");
                    }
                }
            }
            state->players[pid].stamina = 0;
        }
        else if (req.actionType == ACTION_SWAP_IN)
        {
            int storageIndex = req.targetId;

            int success = swapInWeapon(state->players[pid].inventory, storageIndex);

            if (success == 1)
            {
                char logText[100];
                sprintf(logText, "Player %d swapped in a weapon", pid);
                addActionLog(state, logText);
            }
            else
            {
                char logText[100];
                sprintf(logText, "Player %d failed to swap in weapon", pid);
                addActionLog(state, logText);
            }
            state->players[pid].stamina = 0;
        }
        else if (req.actionType == ACTION_ULTIMATE)
        {
            if (playerHasSolarAndLunar(state, pid) == 1 && g_aspPid > 0)
            {
                state->ultimateActive = 1;

                kill(g_aspPid, SIGSTOP);
                alarm(10);

                addActionLog(state, "ULTIMATE: Strategic Process suspended for 10 seconds");

                cout << "[Arbiter] Player " << pid
                     << " used ULTIMATE. ASP suspended for 10 seconds." << endl;
            }
            else
            {
                addActionLog(state, "Ultimate failed: Solar Core and Lunar Blade required");

                cout << "[Arbiter] Player " << pid
                     << " tried Ultimate but does not have both artifacts." << endl;
            }
        }
        else if (req.actionType == ACTION_SKIP)
        {
            char logText[100];
            sprintf(logText, "Player %d skipped turn", pid);
            addActionLog(state, logText);

            cout << "[Arbiter] Player " << pid << " skipped." << endl;
        }

        if (req.actionType == ACTION_SKIP)
        {
            state->players[pid].stamina = 50;
        }
        else
        {
            state->players[pid].stamina = 0;
        }
    }
    else if (req.entityType == ENTITY_ENEMY)
    {
        int eid = req.entityId;

        if (req.actionType == ACTION_STRIKE)
        {
            int tid = req.targetId;

            if (tid >= 0 && tid < state->playerCount && state->players[tid].alive == 1)
            {
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

                if (state->players[tid].hp <= 0)
                {
                    state->players[tid].hp = 0;
                    state->players[tid].alive = 0;
                    state->players[tid].stamina = 0;
                    cout << "[Arbiter] Player " << tid << " defeated!" << endl;
                }
            }
        }
        else
        {
            state->lastNpcActionEnemyId = eid;
            state->lastNpcActionType = ACTION_SKIP;
            state->lastNpcTargetPlayerId = -1;

            char logText[100];
            sprintf(logText, "Enemy %d skipped turn", eid);
            addActionLog(state, logText);

            cout << "[Arbiter] Enemy " << eid << " skipped." << endl;
        }

        if (req.actionType == ACTION_SKIP)
        {
            state->enemies[eid].stamina = 50;
        }
        else
        {
            state->enemies[eid].stamina = 0;
        }
    }

    int status = checkGameOver(state);
    if (status != GAME_RUNNING)
    {
        state->gameStatus = status;

        if (status == GAME_WIN)
        {
            cout << "[Arbiter] *** GAME WIN *** Players killed 10 enemies!" << endl;
        }
        else if (status == GAME_LOSE)
        {
            cout << "[Arbiter] *** GAME LOSE *** All players are dead." << endl;
        }
    }

    state->currentTurnType = ENTITY_NONE;
    state->currentTurnId = -1;

    sem_post(&state->stateLock);
    sem_post(&state->actionDone);
}

// ─────────────────────────────────────────────
// Arrival-time scheduler helpers
// Must be called with stateLock held.
// This follows the rubric rule: stamina fills concurrently, but the first
// entity whose remaining stamina finishes earliest gets the serial turn.
// ─────────────────────────────────────────────
int ceilDivPositive(int numerator, int denominator)
{
    if (numerator <= 0)
        return 0;
    return (numerator + denominator - 1) / denominator;
}

int findReadyActor(SharedState *state, int &outType, int &outId)
{
    for (int i = 0; i < state->playerCount; i++)
    {
        if (state->players[i].alive == 1 && state->players[i].stamina >= PLAYER_MAX_STAMINA)
        {
            outType = ENTITY_PLAYER;
            outId = i;
            return 1;
        }
    }

    for (int i = 0; i < state->enemyCount; i++)
    {
        if (state->enemies[i].alive == 1 && state->enemies[i].stamina >= ENEMY_MAX_STAMINA)
        {
            outType = ENTITY_ENEMY;
            outId = i;
            return 1;
        }
    }

    return 0;
}

int computeNextArrivalSeconds(SharedState *state)
{
    int best = 1000000;

    for (int i = 0; i < state->playerCount; i++)
    {
        if (state->players[i].alive == 0)
            continue;
        int needed = PLAYER_MAX_STAMINA - state->players[i].stamina;
        int seconds = ceilDivPositive(needed, state->players[i].speed);
        if (seconds < best)
            best = seconds;
    }

    for (int i = 0; i < state->enemyCount; i++)
    {
        if (state->enemies[i].alive == 0)
            continue;
        int needed = ENEMY_MAX_STAMINA - state->enemies[i].stamina;
        int seconds = ceilDivPositive(needed, state->enemies[i].speed);
        if (seconds < best)
            best = seconds;
    }

    if (best == 1000000)
        return 1;
    if (best < 0)
        return 0;
    return best;
}

void advanceStaminaBy(SharedState *state, int seconds)
{
    if (seconds < 0)
        seconds = 0;

    for (int i = 0; i < state->playerCount; i++)
    {
        if (state->players[i].alive == 0)
            continue;
        state->players[i].stamina += state->players[i].speed * seconds;
        if (state->players[i].stamina > PLAYER_MAX_STAMINA)
        {
            state->players[i].stamina = PLAYER_MAX_STAMINA;
        }
    }

    for (int i = 0; i < state->enemyCount; i++)
    {
        if (state->enemies[i].alive == 0)
            continue;
        state->enemies[i].stamina += state->enemies[i].speed * seconds;
        if (state->enemies[i].stamina > ENEMY_MAX_STAMINA)
        {
            state->enemies[i].stamina = ENEMY_MAX_STAMINA;
        }
    }
}

// ─────────────────────────────────────────────
// Wait for an action on the actionReady semaphore
// with a timeout of timeoutSecs seconds.
// Returns 1 if action arrived, 0 if timed out.
// ─────────────────────────────────────────────
int waitForActionWithTimeout(SharedState *state, int timeoutSecs)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutSecs;

    int ret = sem_timedwait(&state->actionReady, &ts);

    if (ret == 0)
    {
        return 1; // got action
    }

    if (errno == ETIMEDOUT)
    {
        return 0; // timed out
    }

    // Other error (e.g. interrupted) — treat as timeout
    return 0;
}

// ─────────────────────────────────────────────
// Handle an enemy's turn
// Signals ASP (via currentTurnType/Id) and waits 3 seconds
// If no response, auto-skip
// ─────────────────────────────────────────────
void handleEnemyTurn(SharedState *state, int enemyId)
{
    cout << "[Arbiter] Enemy " << enemyId << "'s turn." << endl;

    // Set turn so ASP knows who to move
    sem_wait(&state->stateLock);
    state->currentTurnType = ENTITY_ENEMY;
    state->currentTurnId = enemyId;
    sem_post(&state->stateLock);

    // Wait up to 3 seconds for ASP to submit an action
    int got = waitForActionWithTimeout(state, 3);

    if (got == 0)
    {
        // Timeout: auto-skip
        cout << "[Arbiter] Enemy " << enemyId
             << " timed out. Auto-skip applied." << endl;

        sem_wait(&state->stateLock);
        state->request.ready = 1;
        state->request.entityType = ENTITY_ENEMY;
        state->request.entityId = enemyId;
        state->request.actionType = ACTION_SKIP;
        state->request.targetType = ENTITY_NONE;
        state->request.targetId = -1;
        sem_post(&state->stateLock);
    }

    processAction(state);

    // Small pause after enemy action so the player can see enemy damage or skip clearly
    sleep(1);
}

// ─────────────────────────────────────────────
// Handle a player's turn
// Sets currentTurn so HIP knows who to prompt,
// then waits on actionReady (no timeout for players)
// ─────────────────────────────────────────────
void handlePlayerTurn(SharedState *state, int playerId)
{
    cout << "[Arbiter] Player " << playerId << "'s turn." << endl;

    sem_wait(&state->stateLock);
    state->currentTurnType = ENTITY_PLAYER;
    state->currentTurnId = playerId;
    sem_post(&state->stateLock);

    // Wait indefinitely for player input
    sem_wait(&state->actionReady);

    processAction(state);
}

// ─────────────────────────────────────────────
// Main scheduling loop
// ─────────────────────────────────────────────
void *staminaTickerThread(void *arg)
{
    SharedState *state = (SharedState *)arg;

    while (g_staminaThreadRunning == 1)
    {
        sleep(1);

        sem_wait(&state->stateLock);

        if (state->gameStatus != GAME_RUNNING)
        {
            sem_post(&state->stateLock);
            break;
        }

        for (int i = 0; i < state->playerCount; i++)
        {
            if (state->players[i].alive == 1)
            {
                state->players[i].stamina += state->players[i].speed;

                if (state->players[i].stamina > PLAYER_MAX_STAMINA)
                {
                    state->players[i].stamina = PLAYER_MAX_STAMINA;
                }
            }
        }

        for (int i = 0; i < state->enemyCount; i++)
        {
            if (state->enemies[i].alive == 1)
            {
                state->enemies[i].stamina += state->enemies[i].speed;

                if (state->enemies[i].stamina > ENEMY_MAX_STAMINA)
                {
                    state->enemies[i].stamina = ENEMY_MAX_STAMINA;
                }
            }
        }

        sem_post(&state->stateLock);
    }

    return NULL;
}
void runGameLoop(SharedState *state)
{
    cout << "[Arbiter] Game loop started." << endl;

    pthread_t staminaThread;
    pthread_create(&staminaThread, NULL, staminaTickerThread, state);

    while (true)
    {
        int actorType = ENTITY_NONE;
        int actorId = -1;

        sem_wait(&state->stateLock);

        if (state->gameStatus != GAME_RUNNING)
        {
            sem_post(&state->stateLock);
            break;
        }

        if (state->currentTurnType == ENTITY_NONE)
        {
            findReadyActor(state, actorType, actorId);
        }

        sem_post(&state->stateLock);

        if (actorType == ENTITY_PLAYER)
        {
            handlePlayerTurn(state, actorId);
        }
        else if (actorType == ENTITY_ENEMY)
        {
            handleEnemyTurn(state, actorId);
        }
        else
        {
            usleep(50000);
        }
    }

    g_staminaThreadRunning = 0;
    pthread_join(staminaThread, NULL);

    cout << "[Arbiter] Game loop ended. Status: " << g_state->gameStatus << endl;
}
// ─────────────────────────────────────────────
// Graceful shutdown
// ─────────────────────────────────────────────
void shutdownGame(SharedState *state)
{
    cout << "[Arbiter] Shutting down..." << endl;

    // Wake up any threads blocked on actionDone so they can exit
    sem_post(&state->actionDone);
    sem_post(&state->actionReady);

    // If HIP is running, terminate it
    if (g_hipPid > 0)
    {
        kill(g_hipPid, SIGTERM);
        waitpid(g_hipPid, nullptr, 0);
        cout << "[Arbiter] HIP terminated." << endl;
    }

    // If ASP is running, terminate it
    if (g_aspPid > 0)
    {
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
int main()
{
    cout << "[Arbiter] Starting..." << endl;

    // Register signal handlers
    signal(SIGTERM, handleSigterm);
    signal(SIGALRM, handleSigalrm);

    // Create and initialize shared memory
    SharedState *state = createSharedMemory();

    if (state == nullptr)
    {
        cerr << "[Arbiter] Failed to create shared memory." << endl;
        return 1;
    }

    g_state = state;

    // Initialize semaphores and zero the state
    sem_init(&state->stateLock, 1, 1);
    sem_init(&state->actionReady, 1, 0);
    sem_init(&state->actionDone, 1, 0);

    memset(&state->inputBuffer, 0, sizeof(InputBuffer));
    state->inputBuffer.playerId = -1;
    state->inputBuffer.actionType = ACTION_NONE;
    state->inputBuffer.targetType = ENTITY_NONE;
    state->inputBuffer.targetId = -1;

    state->request.ready = 0;
    state->partySizeSelected = 0;
    state->partySize = 0;
    state->gameInitialized = 0;
    state->gameStatus = GAME_RUNNING;
    state->arbiterPid = getpid();
    state->enemiesKilled = 0;
    state->currentTurnType = ENTITY_NONE;
    state->currentTurnId = -1;
    state->ultimateActive = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        state->npcThreadAlive[i] = 0;
    }

    state->lastNpcActionEnemyId = -1;
    state->lastNpcActionType = ACTION_NONE;
    state->lastNpcTargetPlayerId = -1;
    state->actionLogCount = 0;

    for (int i = 0; i < 5; i++)
    {
        state->actionLog[i][0] = '\0';
    }
    g_hipPid = fork();

    if (g_hipPid < 0)
    {
        cerr << "[Arbiter] Failed to fork HIP." << endl;
        shutdownGame(state);
        return 1;
    }

    if (g_hipPid == 0)
    {
        execl("./bin/hip", "./bin/hip", NULL);
        cerr << "[HIP Child] execl failed." << endl;
        exit(1);
    }

    cout << "[Arbiter] HIP forked with PID " << g_hipPid << endl;
    cout << "[Arbiter] Shared memory created. Waiting for HIP to select party size..." << endl;

    // ── Wait for HIP to set party size ──────────────────────────────
    while (state->partySizeSelected == 0)
    {
        usleep(100000);

        // Check if game was quit before even starting
        if (state->gameStatus == GAME_QUIT)
        {
            shutdownGame(state);
            return 0;
        }
    }

    cout << "[Arbiter] Party size selected: " << state->partySize << endl;

    // ── Initialize entities with roll-number-based stats ────────────
    initEntities(state);
    g_aspPid = fork();

    if (g_aspPid < 0)
    {
        cerr << "[Arbiter] Failed to fork ASP." << endl;
        shutdownGame(state);
        return 1;
    }

    if (g_aspPid == 0)
    {
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