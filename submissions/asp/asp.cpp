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

    cout << "ASP attached to shared memory." << endl;
    cout << "ASP waiting for game initialization..." << endl;

    while (state->gameInitialized == 0) {
        usleep(100000);
    }

    sem_wait(&state->stateLock);

    cout << "\nASP can read initialized enemies:" << endl;
    cout << "Enemy Count: " << state->enemyCount << endl;

    for (int i = 0; i < state->enemyCount; i++) {
        cout << "Enemy " << i
             << " HP: " << state->enemies[i].hp
             << " Damage: " << state->enemies[i].damage
             << " Speed: " << state->enemies[i].speed
             << endl;
    }

    sem_post(&state->stateLock);

    detachSharedMemory(state);

    cout << "ASP finished initialization test." << endl;

    return 0;
}