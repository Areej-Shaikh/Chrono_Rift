#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <cstdlib>
#include <ctime>
#include <signal.h>


#include "shared_memory.h"
#include "shared_state.h"

using namespace std;

SharedState* state = NULL;
int aspStunned = 0;
int chooseAlivePlayer() {
    int alivePlayers[MAX_PLAYERS];
    int count = 0;

    sem_wait(&state->stateLock);

    for (int i = 0; i < state->playerCount; i++) {
        if (state->players[i].alive == 1) {
            alivePlayers[count] = i;
            count++;
        }
    }

    sem_post(&state->stateLock);

    if (count == 0) {
        return -1;
    }

    return alivePlayers[rand() % count];
}

void submitEnemyMove(int enemyId) {
    int targetPlayer = chooseAlivePlayer();

    sem_wait(&state->stateLock);

    if (state->gameStatus != GAME_RUNNING) {
        sem_post(&state->stateLock);
        return;
    }

    if (state->enemies[enemyId].alive == 0) {
        sem_post(&state->stateLock);
        return;
    }

    state->request.ready = 1;
    state->request.entityType = ENTITY_ENEMY;
    state->request.entityId = enemyId;

    if (targetPlayer == -1) {
        state->request.actionType = ACTION_SKIP;
        state->request.targetType = ENTITY_NONE;
        state->request.targetId = -1;
    }
    else {
        int choice = rand() % 100;

        if (choice < 80) {
            state->request.actionType = ACTION_STRIKE;
            state->request.targetType = ENTITY_PLAYER;
            state->request.targetId = targetPlayer;
        }
        else {
            state->request.actionType = ACTION_SKIP;
            state->request.targetType = ENTITY_NONE;
            state->request.targetId = -1;
        }
    }

    cout << "[ASP] Enemy " << enemyId << " submitted move." << endl;

    sem_post(&state->stateLock);
    sem_post(&state->actionReady);
}

void stunHandler(int sig) {
    (void)sig;
    aspStunned = 1;
    cout << "[ASP] STUN received. Pausing enemies for 3 seconds." << endl;
    alarm(3);
}

void stunRecoveryHandler(int sig) {
    (void)sig;
    aspStunned = 0;
    cout << "[ASP] STUN ended. Enemies resumed." << endl;
}
void* enemyThreadFunction(void* arg) {
    int enemyId = *((int*)arg);

    while (true) {
        while (aspStunned == 1) {
    pause();
}
        sem_wait(&state->stateLock);

        int status = state->gameStatus;
        int currentType = state->currentTurnType;
        int currentId = state->currentTurnId;
        int alive = state->enemies[enemyId].alive;

        sem_post(&state->stateLock);

        if (status != GAME_RUNNING) {
            break;
        }

        if (alive == 1 && currentType == ENTITY_ENEMY && currentId == enemyId) {
            submitEnemyMove(enemyId);

            while (true) {
                sem_wait(&state->stateLock);

                int stillMyTurn = (state->currentTurnType == ENTITY_ENEMY &&
                                   state->currentTurnId == enemyId);
                int gameRunning = (state->gameStatus == GAME_RUNNING);

                sem_post(&state->stateLock);

                if (stillMyTurn == 0 || gameRunning == 0) {
                    break;
                }

                usleep(50000);
            }
        }

        usleep(50000);
    }

    cout << "[ASP] Enemy thread " << enemyId << " exiting." << endl;
    return NULL;
}

int main() {
    srand(time(NULL) ^ getpid());

    state = attachSharedMemory();

    if (state == NULL) {
        cout << "[ASP] Failed to attach shared memory." << endl;
        return 1;
    }

    cout << "[ASP] Attached to shared memory." << endl;

    while (state->gameInitialized == 0) {
        usleep(100000);
    }

    sem_wait(&state->stateLock);
    int enemyCount = state->enemyCount;
    sem_post(&state->stateLock);
signal(SIGUSR1, stunHandler);
signal(SIGALRM, stunRecoveryHandler);
    pthread_t enemyThreads[MAX_ENEMIES];
    int enemyIds[MAX_ENEMIES];

    for (int i = 0; i < enemyCount; i++) {
        enemyIds[i] = i;
        pthread_create(&enemyThreads[i], NULL, enemyThreadFunction, &enemyIds[i]);
    }

    for (int i = 0; i < enemyCount; i++) {
        pthread_join(enemyThreads[i], NULL);
    }

    detachSharedMemory(state);

    cout << "[ASP] Exiting." << endl;
    return 0;
}