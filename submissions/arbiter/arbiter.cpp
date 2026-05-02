#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

#include "shared_memory.h"

using namespace std;

const int ROLL_NO = 620;

int getLastDigit(int number) {
    return number % 10;
}

int getSecondLastDigit(int number) {
    return (number / 10) % 10;
}

int getLastTwoDigits(int number) {
    return number % 100;
}

int randomInRange(int minValue, int maxValue) {
    return minValue + rand() % (maxValue - minValue + 1);
}

void initializePlayersAndEnemies(SharedState* state) {
    int partySize = state->partySize;

    state->playerCount = partySize;

    for (int i = 0; i < state->playerCount; i++) {
        int hp = ROLL_NO + randomInRange(100, 1000);

        state->players[i].id = i;
        state->players[i].hp = hp;
        state->players[i].maxHp = hp;
        state->players[i].damage = getLastDigit(ROLL_NO) + 10;
        state->players[i].speed = 100 / partySize;
        state->players[i].stamina = 0;
        state->players[i].alive = 1;
    }

    state->enemyCount = randomInRange(2, 9);

    for (int i = 0; i < state->enemyCount; i++) {
        int hp = getLastTwoDigits(ROLL_NO) + randomInRange(50, 200);

        state->enemies[i].id = i;
        state->enemies[i].hp = hp;
        state->enemies[i].maxHp = hp;
        state->enemies[i].damage = getSecondLastDigit(ROLL_NO) + 10;
        state->enemies[i].speed = randomInRange(10, 30);
        state->enemies[i].stamina = 0;
        state->enemies[i].alive = 1;
    }

    state->currentTurnType = ENTITY_PLAYER;
state->currentTurnId = 0;

state->gameInitialized = 1;
}

void printGameData(SharedState* state) {
    cout << "\n===== Initialized Game Data =====" << endl;

    cout << "\nPlayers: " << state->playerCount << endl;

    for (int i = 0; i < state->playerCount; i++) {
        cout << "Player " << i
             << " HP: " << state->players[i].hp
             << " Damage: " << state->players[i].damage
             << " Speed: " << state->players[i].speed
             << " Max Stamina: " << PLAYER_MAX_STAMINA
             << endl;
    }

    cout << "\nEnemies: " << state->enemyCount << endl;

    for (int i = 0; i < state->enemyCount; i++) {
        cout << "Enemy " << i
             << " HP: " << state->enemies[i].hp
             << " Damage: " << state->enemies[i].damage
             << " Speed: " << state->enemies[i].speed
             << " Max Stamina: " << ENEMY_MAX_STAMINA
             << endl;
    }

    cout << "=================================\n" << endl;
}

int main() {
    srand(ROLL_NO);

    SharedState* state = createSharedMemory();

    if (state == NULL) {
        cout << "Arbiter failed to create shared memory." << endl;
        return 1;
    }

    initializeSharedState(state);

    cout << "Arbiter created shared memory." << endl;
    cout << "Waiting for HIP to select party size..." << endl;

    while (state->partySizeSelected == 0) {
        usleep(100000);
    }

    sem_wait(&state->stateLock);

    cout << "Party size received from HIP: " << state->partySize << endl;

    initializePlayersAndEnemies(state);
    printGameData(state);

    sem_post(&state->stateLock);

    cout << "Arbiter running as backend only. No SFML window here." << endl;

    while (state->gameStatus == GAME_RUNNING) {
        int hasAction = 0;

        if (sem_trywait(&state->actionReady) == 0) {
            hasAction = 1;
        }

        if (hasAction == 1) {
            sem_wait(&state->stateLock);

            if (state->request.ready == 1 &&
                state->request.entityType == ENTITY_PLAYER) {

                int playerId = state->request.entityId;
                int actionType = state->request.actionType;
                int targetId = state->request.targetId;

                if (actionType == ACTION_STRIKE) {
                    if (targetId >= 0 &&
                        targetId < state->enemyCount &&
                        state->enemies[targetId].alive == 1) {

                        state->enemies[targetId].hp -= state->players[playerId].damage;

                        if (state->enemies[targetId].hp <= 0) {
                            state->enemies[targetId].hp = 0;
                            state->enemies[targetId].alive = 0;
                            state->enemiesKilled++;
                        }

                        state->players[playerId].stamina = 0;

                        cout << "Arbiter: Player " << playerId
                             << " attacked Enemy " << targetId
                             << " for " << state->players[playerId].damage
                             << " damage." << endl;
                    }
                }
                else if (actionType == ACTION_SKIP) {
                    state->players[playerId].stamina = PLAYER_MAX_STAMINA / 2;

                    cout << "Arbiter: Player " << playerId
                         << " skipped." << endl;
                }

                state->request.ready = 0;

                int nextPlayer = playerId + 1;

                if (nextPlayer < state->playerCount) {
                    state->currentTurnType = ENTITY_PLAYER;
                    state->currentTurnId = nextPlayer;
                }
                else {
                    state->currentTurnType = ENTITY_NONE;
                    state->currentTurnId = -1;
                }
            }

            sem_post(&state->stateLock);

            sem_post(&state->actionDone);
        }

        usleep(100000);
    }

    detachSharedMemory(state);
    destroySharedMemory();

    cout << "Arbiter stopped." << endl;

    return 0;
}