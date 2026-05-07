#ifndef ENEMY_ACTIONS_H
#define ENEMY_ACTIONS_H

#include "shared_state.h"

int chooseAlivePlayer(SharedState *state);
void submitEnemyMove(int enemyId, SharedState *state);

#endif
