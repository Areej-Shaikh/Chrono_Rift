#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>

#include "shared_memory.h"

using namespace std;

SharedState* createSharedMemory() {
    shm_unlink(SHM_NAME);

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);

    if (fd == -1) {
        perror("shm_open create");
        return NULL;
    }

    if (ftruncate(fd, sizeof(SharedState)) == -1) {
        perror("ftruncate");
        close(fd);
        return NULL;
    }

    void* ptr = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    if (ptr == MAP_FAILED) {
        perror("mmap create");
        return NULL;
    }

    SharedState* state = (SharedState*)ptr;
    memset(state, 0, sizeof(SharedState));

    return state;
}

SharedState* attachSharedMemory() {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);

    if (fd == -1) {
        perror("shm_open attach");
        return NULL;
    }

    void* ptr = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    if (ptr == MAP_FAILED) {
        perror("mmap attach");
        return NULL;
    }

    return (SharedState*)ptr;
}

void detachSharedMemory(SharedState* state) {
    if (state != NULL) {
        munmap(state, sizeof(SharedState));
    }
}

void destroySharedMemory() {
    shm_unlink(SHM_NAME);
}

void initializeSharedState(SharedState* state) {
    sem_init(&state->stateLock, 1, 1);
    sem_init(&state->actionReady, 1, 0);
    sem_init(&state->actionDone, 1, 0);

    state->playerCount = 1;
    state->enemyCount = 2;

    state->players[0].id = 0;
    state->players[0].hp = 200;
    state->players[0].maxHp = 200;
    state->players[0].damage = 20;
    state->players[0].speed = 100;
    state->players[0].stamina = 0;
    state->players[0].alive = 1;

    for (int i = 0; i < state->enemyCount; i++) {
        state->enemies[i].id = i;
        state->enemies[i].hp = 100;
        state->enemies[i].maxHp = 100;
        state->enemies[i].damage = 15;
        state->enemies[i].speed = 20;
        state->enemies[i].stamina = 0;
        state->enemies[i].alive = 1;
    }

    state->currentTurnType = ENTITY_NONE;
    state->currentTurnId = -1;

    state->request.ready = 0;

    state->enemiesKilled = 0;
    state->gameStatus = GAME_RUNNING;
}