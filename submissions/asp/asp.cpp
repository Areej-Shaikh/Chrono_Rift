#include <iostream>
#include <unistd.h>

#include "shared_memory.h"

using namespace std;

int main() {
    sleep(1);

    SharedState* state = attachSharedMemory();

    if (state == NULL) {
        cout << "ASP failed to attach shared memory." << endl;
        return 1;
    }

    cout << "ASP process attached to shared memory." << endl;

    sem_wait(&state->stateLock);

    cout << "ASP can read enemy count: " << state->enemyCount << endl;

    if (state->enemyCount > 0) {
        cout << "Enemy 0 HP from ASP: " << state->enemies[0].hp << endl;
    }

    sem_post(&state->stateLock);

    detachSharedMemory(state);

    cout << "ASP process finished test." << endl;

    return 0;
}