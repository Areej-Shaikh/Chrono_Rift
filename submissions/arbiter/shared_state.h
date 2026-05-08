#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <semaphore.h>

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

const int ENTITY_NONE = 0;
const int ENTITY_PLAYER = 1;
const int ENTITY_ENEMY = 2;

const int ACTION_NONE = 0;
const int ACTION_STRIKE = 1;
const int ACTION_SKIP = 2;
const int ACTION_EXHAUST = 3;
const int ACTION_HEAL = 4;

const int GAME_RUNNING = 0;
const int GAME_WIN = 1;
const int GAME_LOSE = 2;
const int GAME_QUIT = 3;
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
    sem_t stateLock;
    sem_t actionReady;
    sem_t actionDone;
    int arbiterPid;
    int partySizeSelected;
    int partySize;
    int ultimateActive;
    int gameInitialized;

    int playerCount;
    int enemyCount;

    Player players[MAX_PLAYERS];
    Enemy enemies[MAX_ENEMIES];

    int currentTurnType;
    int currentTurnId;

    InputBuffer inputBuffer;
    ActionRequest request;

    int enemiesKilled;
    int gameStatus;
    int npcThreadAlive[MAX_ENEMIES];
    int lastNpcActionEnemyId;
    int lastNpcActionType;
    int lastNpcTargetPlayerId;
    char actionLog[10][100];
    int actionLogCount;
};

#endif