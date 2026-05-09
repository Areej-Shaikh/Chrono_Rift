

#include <iostream>
#include <unistd.h>
#include "deadlock_monitor.h"
#include "artifact_manager.h"

using namespace std;


static int dfs(ArtifactTable *table, int node, int visited[], int inStack[], int parent[])
{
    visited[node] = 1;
    inStack[node] = 1;

    int next = table->entries[node].waitingFor; 

    if (next >= 0 && next < ARTIFACT_COUNT && table->entries[next].introduced)
    {
        parent[next] = node;
        if (!visited[next])
        {
            int cycle = dfs(table, next, visited, inStack, parent);
            if (cycle != -1)
                return cycle;
        }
        else if (inStack[next])
        {
            
            return next;
        }
    }

    inStack[node] = 0;
    return -1;
}


int checkAndResolveDeadlock(ArtifactTable *table)
{
    sem_wait(&table->artLock);

    
    int holder[ARTIFACT_COUNT];
    int waitingFor[ARTIFACT_COUNT];
    int introduced[ARTIFACT_COUNT];

    for (int i = 0; i < ARTIFACT_COUNT; i++)
    {
        holder[i] = table->entries[i].holder;
        waitingFor[i] = table->entries[i].waitingFor;
        introduced[i] = table->entries[i].introduced;
    }

    sem_post(&table->artLock);

    
    int visited[ARTIFACT_COUNT] = {0};
    int inStack[ARTIFACT_COUNT] = {0};
    int parent[ARTIFACT_COUNT];
    for (int i = 0; i < ARTIFACT_COUNT; i++)
        parent[i] = -1;

    int cycleStart = -1;

    for (int i = 0; i < ARTIFACT_COUNT; i++)
    {
        if (!introduced[i])
            continue; 
        if (holder[i] == ARTIFACT_HOLDER_NONE)
            continue; 
        if (waitingFor[i] == -1)
            continue; 
        if (visited[i])
            continue;

        
        cycleStart = dfs(table, i, visited, inStack, parent);
        if (cycleStart != -1)
            break;
    }

    if (cycleStart == -1)
    {
        return 0; 
    }

    
    cout << "[DeadlockMonitor] Circular wait detected involving artifact "
         << cycleStart << " (" << table->entries[cycleStart].name << ")." << endl;
    printArtifactTable(table);

    
    int victim = forceReleaseArtifact(table, cycleStart);

    if (victim == ARTIFACT_HOLDER_NONE)
    {
        cout << "[DeadlockMonitor] Artifact was already free — no action needed." << endl;
        return 0;
    }

    if (isPlayerHolder(victim))
    {
        cout << "[DeadlockMonitor] Forced Player " << victim
             << " to release " << table->entries[cycleStart].name
             << " to break deadlock." << endl;
    }
    else
    {
        cout << "[DeadlockMonitor] Forced Enemy " << decodeEnemyHolder(victim)
             << " to release " << table->entries[cycleStart].name
             << " to break deadlock." << endl;
    }

    return 1;
}


void *deadlockMonitorThread(void *arg)
{
    DeadlockMonitorArg *marg = (DeadlockMonitorArg *)arg;
    ArtifactTable *table = marg->table;
    volatile int *stopFlag = marg->stopFlag;

    cout << "[DeadlockMonitor] Background monitor thread started." << endl;

    while (!(*stopFlag))
    {
        sleep(1); 

        if (*stopFlag)
            break;

        checkAndResolveDeadlock(table);
    }

    cout << "[DeadlockMonitor] Monitor thread exiting." << endl;
    return nullptr;
}