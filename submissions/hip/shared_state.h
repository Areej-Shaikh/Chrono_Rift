#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <semaphore.h>
#include <pthread.h>
#include "artifact_table.h"

const int MAX_PLAYERS = 4;
const int MAX_ENEMIES = 9;
const int INVENTORY_SIZE = 20;
const int MAX_WEAPONS = 50;
const int MAX_STORAGE = 50;

const int ACTION_USE_WEAPON = 5;
const int ACTION_SWAP_IN = 6;

struct Weapon
{
    char name[30];
    int slotSize;
    int damage;
    int isArtifact;
};

struct InventoryWeapon
{
    Weapon weapon;
    int startSlot;
    int active;
};

struct PlayerInventory
{
    int slots[INVENTORY_SIZE];
    InventoryWeapon weapons[MAX_WEAPONS];
    Weapon longTermStorage[MAX_STORAGE];
    int weaponCount;
    int storageCount;
};

const int PLAYER_MAX_STAMINA = 100;
const int ENEMY_MAX_STAMINA = 150;

const int ENTITY_NONE   = 0;
const int ENTITY_PLAYER = 1;
const int ENTITY_ENEMY  = 2;

const int ACTION_NONE    = 0;
const int ACTION_STRIKE  = 1;
const int ACTION_SKIP    = 2;
const int ACTION_EXHAUST = 3;
const int ACTION_HEAL    = 4;

const int GAME_RUNNING = 0;
const int GAME_WIN     = 1;
const int GAME_LOSE    = 2;
const int GAME_QUIT    = 3;

#define ACTION_ULTIMATE 7

struct Player
{
    int id;
    int hp;
    int maxHp;
    int damage;
    int speed;
    int stamina;
    int alive;
    PlayerInventory inventory;
    int stunned;
};

struct Enemy
{
    int id;
    int hp;
    int maxHp;
    int damage;
    int speed;
    int stamina;
    int alive;
    int stunned;
    int hasWeapon; 
    Weapon heldWeapon; 
};

struct ActionRequest
{
    int ready;
    int entityType;
    int entityId;
    int actionType;
    int targetType;
    int targetId;
};

struct InputBuffer
{
    int hasInput;
    int playerId;
    int actionType;
    int targetType;
    int targetId;
};

struct SharedState
{
    // ── Primary synchronization ───────────────────────────────────────────
    sem_t stateLock;    // Guards all shared state reads/writes
    sem_t actionReady;  // Signals Arbiter that an action is queued
    sem_t actionDone;   // Signals actor that Arbiter has applied the action

    // ── Enemy thread death notification ───────────────────────────────────
    // pthread_cond_timedwait requires a pthread_mutex_t — it cannot use a
    // sem_t. This mutex is ONLY used together with threadCleanupCond.
    // It does NOT replace stateLock for general shared-memory protection.
    pthread_mutex_t threadCleanupMutex;
    pthread_cond_t  threadCleanupCond;
int dropPending;
Weapon pendingDrop;
int dropPlayerId;
int dropEnemyId;
int dropChoice;   // -1 waiting, 0 reject, 1 accept
    // ── Process IDs ──────────────────────────────────────────────────────
    int arbiterPid;

    // ── Game initialization handshake ─────────────────────────────────────
    int partySizeSelected;
    int partySize;
    int gameInitialized;

    // ── Game state ────────────────────────────────────────────────────────
    int ultimateActive;
    int playerCount;
    int enemyCount;

    Player players[MAX_PLAYERS];
    Enemy  enemies[MAX_ENEMIES];

    int currentTurnType;
    int currentTurnId;

    // Arbiter writes this before sending SIGUSR1 so the ASP handler knows
    // which enemy to stun (currentTurnId may be the attacking player at that
    // moment, not the enemy being hit).
    int stunTargetId;

    InputBuffer   inputBuffer;
    ActionRequest request;

    int enemiesKilled;
    int gameStatus;

    // ── NPC thread liveness tracking ──────────────────────────────────────
    int npcThreadAlive[MAX_ENEMIES];

    // ── Last NPC action (for UI feedback) ────────────────────────────────
    int lastNpcActionEnemyId;
    int lastNpcActionType;
    int lastNpcTargetPlayerId;

    // ── Action log (ring buffer, index 0 = newest) ───────────────────────
    char actionLog[10][100];
    int  actionLogCount;

    // ── Artifact system (spec Section 7) ─────────────────────────────────
    ArtifactTable artifactTable;
};

#endif