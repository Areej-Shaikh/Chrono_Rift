#include <iostream>
#include <unistd.h>

#include "shared_memory.h"

using namespace std;

int main() {
    sleep(1);

    SharedState* state = attachSharedMemory();

    if (state == NULL) {
        cout << "HIP failed to attach shared memory." << endl;
        return 1;
    }

    cout << "HIP process attached to shared memory." << endl;

    sem_wait(&state->stateLock);

    cout << "HIP can read player count: " << state->playerCount << endl;

    if (state->playerCount > 0) {
        cout << "Player 0 HP from HIP: " << state->players[0].hp << endl;
    }

    sem_post(&state->stateLock);

    detachSharedMemory(state);

    cout << "HIP process finished test." << endl;

    return 0;
}