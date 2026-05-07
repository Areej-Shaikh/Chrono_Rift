#include <cstdlib>
#include <iostream>

#include "enemy_actions.h"

int chooseAlivePlayer(SharedState *state)
{
    int alivePlayers[MAX_PLAYERS];
    int count = 0;

    sem_wait(&state->stateLock);

    for (int i = 0; i < state->playerCount; i++)
    {
        if (state->players[i].alive == 1)
        {
            alivePlayers[count] = i;
            count++;
        }
    }

    sem_post(&state->stateLock);

    if (count == 0)
    {
        return -1;
    }

    return alivePlayers[rand() % count];
}

void submitEnemyMove(int enemyId, SharedState *state)
{
    int targetPlayer = chooseAlivePlayer(state);

    sem_wait(&state->stateLock);

    if (state->gameStatus != GAME_RUNNING)
    {
        sem_post(&state->stateLock);
        return;
    }

    if (enemyId < 0 || enemyId >= state->enemyCount || state->enemies[enemyId].alive == 0)
    {
        sem_post(&state->stateLock);
        return;
    }

    state->request.ready = 1;
    state->request.entityType = ENTITY_ENEMY;
    state->request.entityId = enemyId;

    if (targetPlayer == -1)
    {
        state->request.actionType = ACTION_SKIP;
        state->request.targetType = ENTITY_NONE;
        state->request.targetId = -1;
    }
    else
    {
        int choice = rand() % 100;

        if (choice < 80)
        {
            state->request.actionType = ACTION_STRIKE;
            state->request.targetType = ENTITY_PLAYER;
            state->request.targetId = targetPlayer;
        }
        else
        {
            state->request.actionType = ACTION_SKIP;
            state->request.targetType = ENTITY_NONE;
            state->request.targetId = -1;
        }
    }

    std::cout << "[ASP] Enemy " << enemyId << " submitted move." << std::endl;

    sem_post(&state->stateLock);
    sem_post(&state->actionReady);
}
