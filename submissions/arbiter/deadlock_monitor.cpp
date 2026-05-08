/*
 * deadlock_monitor.cpp — Spec Section 7: Mandatory Monitoring
 *
 * Deadlock scenario from spec:
 *   Player A holds Solar Core, waiting for Lunar Blade.
 *   NPC B holds Lunar Blade, waiting for Solar Core.
 *   → circular wait → neither can proceed.
 *
 * Detection algorithm (cycle detection on wait-for graph):
 *   Nodes  = artifact entries that are held (holder != NONE)
 *   Edge   = entry[i].waitingFor = j  means  "holder of i is waiting for j"
 *
 *   We do a DFS from each artifact. If we reach an artifact we already
 *   visited in this DFS path, a cycle exists.
 *
 *   With only 3 artifacts the graph is tiny — no need for Tarjan/Kosarajus.
 *
 * Resolution:
 *   Force-release the artifact at the START of the cycle (the one whose
 *   holder was the deepest in the DFS). This unblocks the waiting side.
 */

#include <iostream>
#include <unistd.h>
#include "deadlock_monitor.h"
#include "artifact_manager.h"

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// DFS helper — returns the artifact id that forms the back-edge, or -1
// visited[] and inStack[] are indexed by artifact id
// ─────────────────────────────────────────────────────────────────────────────
static int dfs(ArtifactTable *table, int node, int visited[], int inStack[], int parent[])
{
    visited[node] = 1;
    inStack[node] = 1;

    int next = table->entries[node].waitingFor; // -1 if not waiting

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
            // Back edge found — 'next' is the start of the cycle
            return next;
        }
    }

    inStack[node] = 0;
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// checkAndResolveDeadlock
// ─────────────────────────────────────────────────────────────────────────────
int checkAndResolveDeadlock(ArtifactTable *table)
{
    sem_wait(&table->artLock);

    // Build a local snapshot so we can release artLock before printing/acting
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

    // Only consider introduced artifacts that are held AND waiting
    int visited[ARTIFACT_COUNT] = {0};
    int inStack[ARTIFACT_COUNT] = {0};
    int parent[ARTIFACT_COUNT];
    for (int i = 0; i < ARTIFACT_COUNT; i++)
        parent[i] = -1;

    int cycleStart = -1;

    for (int i = 0; i < ARTIFACT_COUNT; i++)
    {
        if (!introduced[i])
            continue; // not in game yet
        if (holder[i] == ARTIFACT_HOLDER_NONE)
            continue; // free
        if (waitingFor[i] == -1)
            continue; // not waiting for anything
        if (visited[i])
            continue;

        // Temporarily wire the snapshot into the table entries for DFS
        // (DFS reads table->entries[].waitingFor directly)
        cycleStart = dfs(table, i, visited, inStack, parent);
        if (cycleStart != -1)
            break;
    }

    if (cycleStart == -1)
    {
        return 0; // no deadlock
    }

    // ── Deadlock detected ──────────────────────────────────────────────────
    cout << "[DeadlockMonitor] Circular wait detected involving artifact "
         << cycleStart << " (" << table->entries[cycleStart].name << ")." << endl;
    printArtifactTable(table);

    // Resolution: force-release the artifact at cycleStart
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

// ─────────────────────────────────────────────────────────────────────────────
// deadlockMonitorThread — runs inside the Arbiter process
// ─────────────────────────────────────────────────────────────────────────────
void *deadlockMonitorThread(void *arg)
{
    DeadlockMonitorArg *marg = (DeadlockMonitorArg *)arg;
    ArtifactTable *table = marg->table;
    volatile int *stopFlag = marg->stopFlag;

    cout << "[DeadlockMonitor] Background monitor thread started." << endl;

    while (!(*stopFlag))
    {
        sleep(1); // check every second

        if (*stopFlag)
            break;

        checkAndResolveDeadlock(table);
    }

    cout << "[DeadlockMonitor] Monitor thread exiting." << endl;
    return nullptr;
}