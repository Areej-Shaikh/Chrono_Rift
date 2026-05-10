#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <ctime>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cerrno>

#include "shared_memory.h"
#include "shared_state.h"
#include "artifact_manager.h"
#include "deadlock_monitor.h"

using namespace std;


const int ROLL_NUMBER = 620;
const int ROLL_LAST2 = 20;
const int ROLL_LAST1 = 0;
const int ROLL_SECOND_LAST = 2;


static SharedState *g_state = nullptr;
static pid_t g_aspPid = -1;
static pid_t g_hipPid = -1;
static pthread_t g_deadlockThread;
static volatile int g_deadlockStop = 0;
void addActionLog(SharedState *state, const char *text);


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
int getArtifactIdFromWeapon(Weapon weapon)
{
    if (strcmp(weapon.name, "Solar Core") == 0)
    {
        return ARTIFACT_SOLAR_CORE;
    }

    if (strcmp(weapon.name, "Lunar Blade") == 0)
    {
        return ARTIFACT_LUNAR_BLADE;
    }

    if (strcmp(weapon.name, "Eclipse Relic") == 0)
    {
        return ARTIFACT_ECLIPSE_RELIC;
    }

    return -1;
}

int isArtifactWeapon(Weapon weapon)
{
    return getArtifactIdFromWeapon(weapon) != -1;
}

void releaseArtifactIfNeeded(SharedState *state, Weapon weapon, int holder)
{
    int artifactId = getArtifactIdFromWeapon(weapon);

    if (artifactId != -1)
    {
        releaseArtifact(&state->artifactTable, artifactId, holder);
    }
    if (artifactId != -1)
    {
        cout << "Releasing artifact: " << weapon.name
             << " from holder " << holder << endl;
    }
}
void releaseAllPlayerArtifacts(SharedState *state, int pid)
{
    for (int i = 0; i < state->players[pid].inventory.weaponCount; i++)
    {
        if (state->players[pid].inventory.weapons[i].active == 1)
        {
            Weapon weapon = state->players[pid].inventory.weapons[i].weapon;
            releaseArtifactIfNeeded(state, weapon, pid);
        }
    }
}
int acquireArtifactIfNeeded(SharedState *state, Weapon weapon, int holder)
{
    int artifactId = getArtifactIdFromWeapon(weapon);

    if (artifactId == -1)
    {
        return 1;
    }

    if (artifactId == ARTIFACT_ECLIPSE_RELIC)
    {
        introduceEclipseRelic(&state->artifactTable);
    }
    if (artifactId != -1)
    {
        cout << "Trying to acquire artifact: " << weapon.name
             << " for holder " << holder << endl;
    }
    return acquireArtifact(&state->artifactTable, artifactId, holder);
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
void removeWeaponToStorage(SharedState *state, PlayerInventory &inv, int weaponIndex, int holder)
{
    if (weaponIndex < 0 || weaponIndex >= inv.weaponCount)
    {
        return;
    }

    if (inv.weapons[weaponIndex].active == 0)
    {
        return;
    }

    Weapon removedWeapon = inv.weapons[weaponIndex].weapon;

    int start = inv.weapons[weaponIndex].startSlot;
    int size = removedWeapon.slotSize;

    for (int i = start; i < start + size; i++)
    {
        inv.slots[i] = -1;
    }

    if (inv.storageCount < MAX_STORAGE)
    {
        inv.longTermStorage[inv.storageCount] = removedWeapon;
        inv.storageCount++;
    }

    inv.weapons[weaponIndex].active = 0;
    inv.weapons[weaponIndex].startSlot = -1;

    
    releaseArtifactIfNeeded(state, removedWeapon, holder);
}
void swapOutEnoughWeapons(SharedState *state, PlayerInventory &inv, int neededSlots, int holder)
{
    while (findContiguousSpace(inv, neededSlots) == -1)
    {
        int bestWeapon = -1;
        int largestSize = -1;

        for (int i = 0; i < inv.weaponCount; i++)
        {
            if (inv.weapons[i].active == 1)
            {
                int size = inv.weapons[i].weapon.slotSize;

                if (size > largestSize)
                {
                    largestSize = size;
                    bestWeapon = i;
                }
            }
        }

        if (bestWeapon == -1)
        {
            break;
        }

        removeWeaponToStorage(state, inv, bestWeapon, holder);
    }
}

int addWeaponToInventory(SharedState *state, PlayerInventory &inv, Weapon weapon, int holder)
{
    if (weapon.slotSize > INVENTORY_SIZE)
    {
        return 0;
    }

    int acquired = acquireArtifactIfNeeded(state, weapon, holder);

    if (acquired == 0)
    {
        return 0;
    }

    int start = findContiguousSpace(inv, weapon.slotSize);

    if (start == -1)
    {
        swapOutEnoughWeapons(state, inv, weapon.slotSize, holder);
        start = findContiguousSpace(inv, weapon.slotSize);
    }

    if (start == -1 || inv.weaponCount >= MAX_WEAPONS)
    {
        releaseArtifactIfNeeded(state, weapon, holder);
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
int swapInWeapon(SharedState *state, PlayerInventory &inv, int storageIndex, int holder)
{
    if (storageIndex < 0 || storageIndex >= inv.storageCount)
    {
        return 0;
    }

    Weapon weapon = inv.longTermStorage[storageIndex];

    int added = addWeaponToInventory(state, inv, weapon, holder);

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
    int r = rand() % 9;

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
    else if (r == 7)
    {
        return createWeapon("Eclipse Relic", 5, 75, 1);
    }
    else if (r == 8)
    {
        return createWeapon("Splinter Stick", 2, 12, 0);
    }
    return createWeapon("Splinter Stick", 2, 12, 0);
}

void handleWeaponDrop(SharedState *state, int killingPlayerId, int defeatedEnemyId)
{
    
    if (defeatedEnemyId >= 0 && defeatedEnemyId < state->enemyCount &&
        state->enemies[defeatedEnemyId].hasWeapon == 1)
    {
        addActionLog(state, "Enemy carried its own weapon — no drop.");
        cout << "[Arbiter] Enemy " << defeatedEnemyId
             << " had its own weapon — no drop." << endl;
        return;
    }

    int dropChance = rand() % 100;

    if (dropChance < 50)
    {
        Weapon dropped = getRandomDroppedWeapon();
        if (defeatedEnemyId >= 0 &&
            defeatedEnemyId < state->enemyCount &&
            state->enemies[defeatedEnemyId].hasWeapon == 1)
        {
            int holder = encodeEnemyHolder(defeatedEnemyId);

            acquireArtifactIfNeeded(
                state,
                dropped,
                holder);
        }

        state->dropPending = 1;
        state->pendingDrop = dropped;
        state->dropPlayerId = killingPlayerId;
        state->dropEnemyId = defeatedEnemyId;
        state->dropChoice = -1;

        char logText[120];
        sprintf(logText, "DROP: %s appeared. Player %d choose Y/N.",
                dropped.name, killingPlayerId + 1);
        addActionLog(state, logText);

        cout << "[Arbiter] " << logText << endl;
    }
}
void processDropChoice(SharedState *state)
{
    if (state->dropPending == 0 || state->dropChoice == -1)
    {
        return;
    }

    int pid = state->dropPlayerId;
    int enemyId = state->dropEnemyId;
    Weapon dropped = state->pendingDrop;

    if (state->dropChoice == 1)
    {
        int added = addWeaponToInventory(state, state->players[pid].inventory, dropped, pid);

        if (added == 1)
        {
            char logText[120];
            sprintf(logText, "Player %d picked up %s", pid + 1, dropped.name);
            addActionLog(state, logText);

            cout << "[Arbiter] " << logText << endl;
        }
        else
        {
            char logText[120];
            sprintf(logText, "Player %d could not pick %s because it is locked", pid + 1, dropped.name);
            addActionLog(state, logText);

            cout << "[Arbiter] " << logText << endl;
        }
    }
    else
    {
        if (enemyId >= 0 && enemyId < state->enemyCount)
        {
            int enemyHolder = encodeEnemyHolder(enemyId);

            int acquired = acquireArtifactIfNeeded(state, dropped, enemyHolder);

            if (acquired == 1)
            {
                state->enemies[enemyId].hasWeapon = 1;
                state->enemies[enemyId].heldWeapon = dropped;

                char logText[120];
                sprintf(logText, "Player refused %s. Enemy picked it up.", dropped.name);
                addActionLog(state, logText);

                cout << "[Arbiter] " << logText << endl;
            }
            else
            {
                state->enemies[enemyId].hasWeapon = 0;

                addActionLog(state, "Enemy could not pick artifact because it is locked");
                cout << "[Arbiter] Enemy could not pick artifact because it is locked" << endl;
            }
        }
    }
    state->dropPending = 0;
    state->dropPlayerId = -1;
    state->dropEnemyId = -1;
    state->dropChoice = -1;
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
    state->enemies[index].stunned = 0;
    
    state->enemies[index].hasWeapon = (rand() % 100 < 30) ? 1 : 0;
}


void initEntities(SharedState *state)
{
    srand(ROLL_NUMBER + time(NULL) + getpid());

    
    int enemyCount = randRange(2, 9);
    state->enemyCount = enemyCount;

    int playerCount = state->partySize;
    state->playerCount = playerCount;

    int playerSpeed = 100 / playerCount;

    for (int i = 0; i < playerCount; i++)
    {
        int hp = ROLL_NUMBER + randRange(100, 1000);

        state->players[i].id = i;
        state->players[i].maxHp = hp;
        state->players[i].hp = hp;
        state->players[i].damage = ROLL_LAST1 + 10; 
        state->players[i].speed = playerSpeed;
        state->players[i].stamina = 0;
        state->players[i].alive = 1;
        initializeInventory(state->players[i].inventory);
        
        
    }

    
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


int checkGameOver(SharedState *state)
{
    
    if (state->enemiesKilled >= 10)
    {
        return GAME_WIN;
    }

    
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
    
    for (int i = 9; i > 0; i--)
    {
        strcpy(state->actionLog[i], state->actionLog[i - 1]);
    }

    strncpy(state->actionLog[0], text, 99);
    state->actionLog[0][99] = '\0';

    if (state->actionLogCount < 10)
    {
        state->actionLogCount++;
    }
}


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

                char logText[100];
                sprintf(logText, "Player %d struck NPC %d for %d damage", pid + 1, tid + 1, dmg);
                addActionLog(state, logText);

                cout << "[Arbiter] Player " << pid
                     << " struck Enemy " << tid
                     << " for " << dmg << " dmg."
                     << " Enemy HP now: " << state->enemies[tid].hp << endl;

                
                if (state->enemies[tid].hp > 0 && rand() % 100 < 25)
                {
                    state->stunTargetId = tid;

                    char stunLog[100];
                    sprintf(stunLog, "NPC %d stunned for 3 seconds", tid + 1);
                    addActionLog(state, stunLog);

                    cout << "[Arbiter] Enemy " << tid << " stunned!" << endl;

                    
                    sem_post(&state->stateLock);
                    kill(state->aspPid, SIGUSR1);
                    sem_wait(&state->stateLock);
                }

                if (state->enemies[tid].hp <= 0)
                {
                    state->enemies[tid].hp = 0;
                    state->enemies[tid].alive = 0;
                    state->enemies[tid].stamina = 0;
                    state->enemiesKilled++;
                    int enemyHolder = encodeEnemyHolder(tid);

                    releaseArtifact(&state->artifactTable, ARTIFACT_SOLAR_CORE, enemyHolder);
                    releaseArtifact(&state->artifactTable, ARTIFACT_LUNAR_BLADE, enemyHolder);
                    releaseArtifact(&state->artifactTable, ARTIFACT_ECLIPSE_RELIC, enemyHolder);
                    handleWeaponDrop(state, pid, tid);
                    cout << "[Arbiter] Enemy " << tid << " defeated! Total kills: "
                         << state->enemiesKilled << endl;

                    if (state->enemiesKilled < 10)
                    {
                        spawnEnemyAt(state, tid);
                        char spawnLog[100];
                        sprintf(spawnLog, "A new NPC %d entered the rift", tid + 1);
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
                sprintf(logText, "Player %d exhausted NPC %d by %d stamina", pid + 1, tid + 1, reduce);
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
            sprintf(logText, "Player %d healed by %d HP", pid + 1, healAmount);
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
                sprintf(logText, "Player %d used %s on NPC %d for %d damage",
                        pid + 1,
                        state->players[pid].inventory.weapons[weaponIndex].weapon.name,
                        tid + 1,
                        dmg);
                addActionLog(state, logText);

                if (state->enemies[tid].hp <= 0)
                {
                    state->enemies[tid].hp = 0;
                    state->enemies[tid].alive = 0;
                    state->enemies[tid].stamina = 0;
                    state->enemiesKilled++;
                    int enemyHolder = encodeEnemyHolder(tid);

                    releaseArtifact(&state->artifactTable, ARTIFACT_SOLAR_CORE, enemyHolder);
                    releaseArtifact(&state->artifactTable, ARTIFACT_LUNAR_BLADE, enemyHolder);
                    releaseArtifact(&state->artifactTable, ARTIFACT_ECLIPSE_RELIC, enemyHolder);
                    handleWeaponDrop(state, pid, tid);
                    if (state->enemiesKilled < 10)
                    {
                        spawnEnemyAt(state, tid);
                        addActionLog(state, "A new NPC entered the rift");
                    }
                }
            }
            state->players[pid].stamina = 0;
        }
        else if (req.actionType == ACTION_SWAP_IN)
        {
            int storageIndex = req.targetId;
            int success = swapInWeapon(state, state->players[pid].inventory, storageIndex, pid);

            if (success == 1)
            {
                char logText[100];
                sprintf(logText, "Player %d swapped in a weapon", pid + 1);
                addActionLog(state, logText);
            }
            else
            {
                char logText[100];
                sprintf(logText, "Player %d failed to swap in weapon", pid + 1);
                addActionLog(state, logText);
            }
            state->players[pid].stamina = 0;
        }
        else if (req.actionType == ACTION_ULTIMATE)
        {
            if (playerHasSolarAndLunar(state, pid) == 1 &&
                holderHasArtifact(&state->artifactTable, ARTIFACT_SOLAR_CORE, pid) == 1 &&
                holderHasArtifact(&state->artifactTable, ARTIFACT_LUNAR_BLADE, pid) == 1 &&
                g_aspPid > 0)
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
            sprintf(logText, "Player %d skipped turn", pid + 1);
            addActionLog(state, logText);

            cout << "[Arbiter] Player " << pid << " skipped." << endl;
        }

        if (req.actionType == ACTION_SKIP)
        {
            state->players[pid].stamina = PLAYER_MAX_STAMINA / 2;
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
if (state->players[tid].hp > 0 && rand() % 100 < 25)
{
    char stunLog[100];
    sprintf(stunLog, "Player %d stunned for 3 seconds", tid + 1);
    addActionLog(state, stunLog);

    cout << "[Arbiter] Player " << tid << " stunned!" << endl;

    sem_post(&state->stateLock);
    kill(state->hipPid, SIGUSR1);
    sem_wait(&state->stateLock);
}
                state->lastNpcActionEnemyId = eid;
                state->lastNpcActionType = ACTION_STRIKE;
                state->lastNpcTargetPlayerId = tid;

                char logText[100];
                sprintf(logText, "NPC %d struck Player %d for %d damage", eid + 1, tid + 1, dmg);
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

                    releaseAllPlayerArtifacts(state, tid);

                    char deathLog[100];
                    sprintf(deathLog, "Player %d defeated. Artifacts released.", tid + 1);
                    addActionLog(state, deathLog);

                    cout << "[Arbiter] " << deathLog << endl;
                }
            }
        }
        else
        {
            state->lastNpcActionEnemyId = eid;
            state->lastNpcActionType = ACTION_SKIP;
            state->lastNpcTargetPlayerId = -1;

            char logText[100];
            sprintf(logText, "NPC %d skipped turn", eid + 1);
            addActionLog(state, logText);

            cout << "[Arbiter] Enemy " << eid << " skipped." << endl;
        }

        if (req.actionType == ACTION_SKIP)
        {
            state->enemies[eid].stamina = ENEMY_MAX_STAMINA / 2;
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


int ceilDivPositive(int numerator, int denominator)
{
    if (numerator <= 0)
        return 0;
    return (numerator + denominator - 1) / denominator;
}


int findReadyActor(SharedState *state, int &outType, int &outId)
{
    int bestTicks = INT32_MAX;
    outType = ENTITY_NONE;
    outId = -1;

    
    for (int i = 0; i < state->playerCount; i++)
    {
        if (state->players[i].alive == 0)
            continue;
        int gap = PLAYER_MAX_STAMINA - state->players[i].stamina;
        int spd = state->players[i].speed;
        int ticks = (gap <= 0) ? 0 : (gap + spd - 1) / spd;

        if (ticks < bestTicks)
        {
            bestTicks = ticks;
            outType = ENTITY_PLAYER;
            outId = i;
        }
    }

    
    for (int i = 0; i < state->enemyCount; i++)
    {
        if (state->enemies[i].alive == 0)
            continue;
        int gap = ENEMY_MAX_STAMINA - state->enemies[i].stamina;
        int spd = state->enemies[i].speed;
        int ticks = (gap <= 0) ? 0 : (gap + spd - 1) / spd;

        if (ticks < bestTicks)
        {
            bestTicks = ticks;
            outType = ENTITY_ENEMY;
            outId = i;
        }
    }

    return (outType != ENTITY_NONE) ? 1 : 0;
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


int waitForActionWithTimeout(SharedState *state, int timeoutSecs)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutSecs;

    int ret = sem_timedwait(&state->actionReady, &ts);

    if (ret == 0)
    {
        return 1; 
    }

    if (errno == ETIMEDOUT)
    {
        return 0; 
    }

    
    return 0;
}


int isGameRunning(SharedState *state)
{
    sem_wait(&state->stateLock);
    int running = (state->gameStatus == GAME_RUNNING);
    sem_post(&state->stateLock);

    return running;
}
void handleEnemyTurn(SharedState *state, int enemyId)
{
    cout << "[Arbiter] Enemy " << enemyId << "'s turn." << endl;

    
    sem_wait(&state->stateLock);
    state->request.ready = 0;
    state->currentTurnType = ENTITY_ENEMY;
    state->currentTurnId = enemyId;
    sem_post(&state->stateLock);

    
    int got = waitForActionWithTimeout(state, 3);

    if (got == 0)
    {
        
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

    if (isGameRunning(state) == 1)
    {
        processAction(state);
    }
}


void handlePlayerTurn(SharedState *state, int playerId)
{
    cout << "[Arbiter] Player " << playerId << "'s turn." << endl;

    sem_wait(&state->stateLock);
    state->currentTurnType = ENTITY_PLAYER;
    state->currentTurnId = playerId;
    sem_post(&state->stateLock);

    
    sem_wait(&state->actionReady);

    if (isGameRunning(state) == 1)
    {
        processAction(state);
    }
}


void runGameLoop(SharedState *state)
{
    cout << "[Arbiter] Game loop started." << endl;

    while (true)
    {
        int actorType = ENTITY_NONE;
        int actorId = -1;
        int ticksToWait = 0;

        sem_wait(&state->stateLock);

        if (state->gameStatus != GAME_RUNNING)
        {
            sem_post(&state->stateLock);
            break;
        }

        
        processDropChoice(state);

        
        if (state->dropPending == 1 && state->dropChoice == -1)
        {
            sem_post(&state->stateLock);

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1; 
            sem_timedwait(&state->dropAnswered, &ts);
            continue;
        }
        
        {
            int bestTicks = INT32_MAX;

            for (int i = 0; i < state->playerCount; i++)
            {
                if (state->players[i].alive == 0)
                    continue;
                int gap = PLAYER_MAX_STAMINA - state->players[i].stamina;
                int spd = state->players[i].speed;
                int ticks = (gap <= 0) ? 0 : (gap + spd - 1) / spd;
                if (ticks < bestTicks)
                {
                    bestTicks = ticks;
                    actorType = ENTITY_PLAYER;
                    actorId = i;
                }
            }
            for (int i = 0; i < state->enemyCount; i++)
            {
                if (state->enemies[i].alive == 0)
                    continue;
                int gap = ENEMY_MAX_STAMINA - state->enemies[i].stamina;
                int spd = state->enemies[i].speed;
                int ticks = (gap <= 0) ? 0 : (gap + spd - 1) / spd;
                if (ticks < bestTicks)
                {
                    bestTicks = ticks;
                    actorType = ENTITY_ENEMY;
                    actorId = i;
                }
            }
            ticksToWait = bestTicks;
        }

        sem_post(&state->stateLock);

        if (actorType == ENTITY_NONE)
        {
            usleep(50000);
            continue;
        }

        
        for (int t = 0; t < ticksToWait; t++)
        {
            sleep(1);

            sem_wait(&state->stateLock);
            if (state->gameStatus != GAME_RUNNING)
            {

                sem_post(&state->stateLock);
                goto gameOver;
            }
            advanceStaminaBy(state, 1);
            sem_post(&state->stateLock);
        }

        
        if (actorType == ENTITY_PLAYER)
        {
            handlePlayerTurn(state, actorId);
        }
        else if (actorType == ENTITY_ENEMY)
        {
            handleEnemyTurn(state, actorId);
        }
    }

gameOver:
    cout << "[Arbiter] Game loop ended. Status: " << g_state->gameStatus << endl;
}


void shutdownGame(SharedState *state)
{
    cout << "[Arbiter] Shutting down..." << endl;

    
    sem_post(&state->actionDone);
    sem_post(&state->actionReady);
    
    sem_post(&state->dropAnswered);

    
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (state->npcThreadAlive[i] == 1)
        {
            sem_post(&state->actionDone); 
            cout << "[Arbiter] Closing channel for NPC thread " << i << endl;
        }
    }

    
    if (g_hipPid > 0)
    {
        kill(g_hipPid, SIGTERM);
        waitpid(g_hipPid, nullptr, 0);
        cout << "[Arbiter] HIP terminated." << endl;
    }

    
    if (g_aspPid > 0)
    {
        kill(g_aspPid, SIGTERM);
        waitpid(g_aspPid, nullptr, 0);
        cout << "[Arbiter] ASP terminated." << endl;
    }

    sleep(1); 

    
    g_deadlockStop = 1;
    pthread_join(g_deadlockThread, nullptr);

    destroySharedMemory();
    cout << "[Arbiter] Shared memory destroyed. Goodbye." << endl;
}


int main()
{
    cout << "[Arbiter] Starting..." << endl;

    
    signal(SIGTERM, handleSigterm);
    signal(SIGALRM, handleSigalrm);

    
    SharedState *state = createSharedMemory();

    if (state == nullptr)
    {
        cerr << "[Arbiter] Failed to create shared memory." << endl;
        return 1;
    }

    g_state = state;

    
    sem_init(&state->stateLock, 1, 1);
    sem_init(&state->actionReady, 1, 0);
    sem_init(&state->actionDone, 1, 0);
    sem_init(&state->dropAnswered, 1, 0); 

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
    state->hipPid = -1;
state->aspPid = -1;
    state->enemiesKilled = 0;
    state->currentTurnType = ENTITY_NONE;
    state->currentTurnId = -1;
    state->ultimateActive = 0;
    state->dropPending = 0;
    state->dropPlayerId = -1;
    state->dropEnemyId = -1;
    state->dropChoice = -1;
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

    
    initArtifactTable(&state->artifactTable);

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

    
    while (state->partySizeSelected == 0)
    {
        usleep(100000);

        
        if (state->gameStatus == GAME_QUIT)
        {
            shutdownGame(state);
            return 0;
        }
    }

    cout << "[Arbiter] Party size selected: " << state->partySize << endl;

    
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

    
    static DeadlockMonitorArg g_deadlockArg;
    g_deadlockArg.table = &state->artifactTable;
    g_deadlockArg.stopFlag = &g_deadlockStop;
    pthread_create(&g_deadlockThread, nullptr, deadlockMonitorThread, &g_deadlockArg);

    
    runGameLoop(state);

    
    shutdownGame(state);

    return 0;
}