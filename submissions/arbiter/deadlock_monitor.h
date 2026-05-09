#ifndef DEADLOCK_MONITOR_H
#define DEADLOCK_MONITOR_H

#include "artifact_table.h"


struct DeadlockMonitorArg
{
    ArtifactTable *table;
    volatile int *stopFlag; 
};


void *deadlockMonitorThread(void *arg);


int checkAndResolveDeadlock(ArtifactTable *table);

#endif 