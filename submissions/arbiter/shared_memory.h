#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include "shared_state.h"

const char SHM_NAME[] = "/chrono_rift_shm";

SharedState* createSharedMemory();
SharedState* attachSharedMemory();
void detachSharedMemory(SharedState* state);
void destroySharedMemory();
void initializeSharedState(SharedState* state);

#endif
