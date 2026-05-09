#include <iostream>
#include <cstring>
#include "artifact_manager.h"

using namespace std;


void initArtifactTable(ArtifactTable *table)
{
    sem_init(&table->artLock, 1, 1); 

    
    table->entries[ARTIFACT_SOLAR_CORE].introduced = 1;
    table->entries[ARTIFACT_SOLAR_CORE].holder = ARTIFACT_HOLDER_NONE;
    table->entries[ARTIFACT_SOLAR_CORE].waitingFor = -1;
    strncpy(table->entries[ARTIFACT_SOLAR_CORE].name, "Solar Core", 29);

    
    table->entries[ARTIFACT_LUNAR_BLADE].introduced = 1;
    table->entries[ARTIFACT_LUNAR_BLADE].holder = ARTIFACT_HOLDER_NONE;
    table->entries[ARTIFACT_LUNAR_BLADE].waitingFor = -1;
    strncpy(table->entries[ARTIFACT_LUNAR_BLADE].name, "Lunar Blade", 29);

    
    table->entries[ARTIFACT_ECLIPSE_RELIC].introduced = 0;
    table->entries[ARTIFACT_ECLIPSE_RELIC].holder = ARTIFACT_HOLDER_NONE;
    table->entries[ARTIFACT_ECLIPSE_RELIC].waitingFor = -1;
    strncpy(table->entries[ARTIFACT_ECLIPSE_RELIC].name, "Eclipse Relic", 29);
}


int acquireArtifact(ArtifactTable *table, int artifactId, int holderEncoded)
{
    if (artifactId < 0 || artifactId >= ARTIFACT_COUNT)
        return 0;

    sem_wait(&table->artLock);

    ArtifactEntry &entry = table->entries[artifactId];

    if (!entry.introduced || entry.holder != ARTIFACT_HOLDER_NONE)
    {
        
        
        for (int i = 0; i < ARTIFACT_COUNT; i++)
        {
            if (table->entries[i].holder == holderEncoded)
            {
                table->entries[i].waitingFor = artifactId;
                break;
            }
        }
        sem_post(&table->artLock);
        return 0;
    }

    
    entry.holder = holderEncoded;
    
    for (int i = 0; i < ARTIFACT_COUNT; i++)
    {
        if (i != artifactId && table->entries[i].holder == holderEncoded)
        {
            table->entries[i].waitingFor = -1;
        }
    }

    sem_post(&table->artLock);
    return 1;
}


int releaseArtifact(ArtifactTable *table, int artifactId, int holderEncoded)
{
    if (artifactId < 0 || artifactId >= ARTIFACT_COUNT)
        return 0;

    sem_wait(&table->artLock);

    ArtifactEntry &entry = table->entries[artifactId];

    if (entry.holder != holderEncoded)
    {
        sem_post(&table->artLock);
        return 0;
    }

    entry.holder = ARTIFACT_HOLDER_NONE;
    entry.waitingFor = -1;

    sem_post(&table->artLock);
    return 1;
}


void introduceEclipseRelic(ArtifactTable *table)
{
    sem_wait(&table->artLock);
    if (!table->entries[ARTIFACT_ECLIPSE_RELIC].introduced)
    {
        table->entries[ARTIFACT_ECLIPSE_RELIC].introduced = 1;
        cout << "[Artifacts] Eclipse Relic introduced into the world." << endl;
    }
    sem_post(&table->artLock);
}


int holderHasArtifact(ArtifactTable *table, int artifactId, int holderEncoded)
{
    if (artifactId < 0 || artifactId >= ARTIFACT_COUNT)
        return 0;

    sem_wait(&table->artLock);
    int result = (table->entries[artifactId].holder == holderEncoded) ? 1 : 0;
    sem_post(&table->artLock);

    return result;
}


int forceReleaseArtifact(ArtifactTable *table, int artifactId)
{
    if (artifactId < 0 || artifactId >= ARTIFACT_COUNT)
        return ARTIFACT_HOLDER_NONE;

    sem_wait(&table->artLock);

    int victim = table->entries[artifactId].holder;
    table->entries[artifactId].holder = ARTIFACT_HOLDER_NONE;
    table->entries[artifactId].waitingFor = -1;

    
    for (int i = 0; i < ARTIFACT_COUNT; i++)
    {
        if (i != artifactId && table->entries[i].holder == victim)
        {
            table->entries[i].waitingFor = -1;
        }
    }

    sem_post(&table->artLock);
    return victim;
}


void printArtifactTable(ArtifactTable *table)
{
    sem_wait(&table->artLock);
    cout << "[ArtifactTable]" << endl;
    for (int i = 0; i < ARTIFACT_COUNT; i++)
    {
        ArtifactEntry &e = table->entries[i];
        if (!e.introduced)
        {
            cout << "  [" << i << "] " << e.name << " — not yet introduced" << endl;
            continue;
        }
        if (e.holder == ARTIFACT_HOLDER_NONE)
        {
            cout << "  [" << i << "] " << e.name << " — FREE" << endl;
        }
        else if (isPlayerHolder(e.holder))
        {
            cout << "  [" << i << "] " << e.name
                 << " — held by Player " << e.holder;
            if (e.waitingFor >= 0)
                cout << "  (waiting for artifact " << e.waitingFor << ")";
            cout << endl;
        }
        else
        {
            cout << "  [" << i << "] " << e.name
                 << " — held by Enemy " << decodeEnemyHolder(e.holder);
            if (e.waitingFor >= 0)
                cout << "  (waiting for artifact " << e.waitingFor << ")";
            cout << endl;
        }
    }
    sem_post(&table->artLock);
}