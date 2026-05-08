#ifndef DEADLOCK_MONITOR_H
#define DEADLOCK_MONITOR_H

#include "artifact_table.h"

// ─────────────────────────────────────────────────────────────────────────────
// Passed to the deadlock monitor thread on creation.
// ─────────────────────────────────────────────────────────────────────────────
struct DeadlockMonitorArg
{
    ArtifactTable *table;
    volatile int *stopFlag; // set to 1 to stop the monitor thread
};

// ─────────────────────────────────────────────────────────────────────────────
// Thread entry point.
// Polls the artifact table every second looking for circular-wait cycles.
// When a deadlock is found, it force-releases one artifact to break the cycle.
// ─────────────────────────────────────────────────────────────────────────────
void *deadlockMonitorThread(void *arg);

// ─────────────────────────────────────────────────────────────────────────────
// Synchronous check (also usable from tests / arbiter directly).
// Scans the table for a circular wait and resolves it if found.
// Returns 1 if a deadlock was detected and resolved, 0 otherwise.
// Caller must NOT hold artLock.
// ─────────────────────────────────────────────────────────────────────────────
int checkAndResolveDeadlock(ArtifactTable *table);

#endif // DEADLOCK_MONITOR_H