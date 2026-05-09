#ifndef ARTIFACT_TABLE_H
#define ARTIFACT_TABLE_H


#define ARTIFACT_SOLAR_CORE 0
#define ARTIFACT_LUNAR_BLADE 1
#define ARTIFACT_ECLIPSE_RELIC 2 
#define ARTIFACT_COUNT 3


#define ARTIFACT_HOLDER_NONE -1 


inline int encodeEnemyHolder(int enemyId) { return -(enemyId + 2); }
inline int decodeEnemyHolder(int encoded) { return -(encoded + 2); }
inline int isEnemyHolder(int h) { return h <= -2; }
inline int isPlayerHolder(int h) { return h >= 0; }


struct ArtifactEntry
{
    int introduced; 
    int holder;     
    int waitingFor; 
                    
                    
    char name[30];
};


#include <semaphore.h>

struct ArtifactTable
{
    sem_t artLock; 
    ArtifactEntry entries[ARTIFACT_COUNT];
};

#endif 