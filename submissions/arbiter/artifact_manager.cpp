#include <iostream>
#include <cstring>
#include "artifact_manager.h"

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// initArtifactTable
// ─────────────────────────────────────────────────────────────────────────────
void initArtifactTable(ArtifactTable *table)
{
    sem_init(&table->artLock, 1, 1); // shared=1 (cross-process), initial=1

    // Solar Core
    table->entries[ARTIFACT_SOLAR_CORE].introduced = 1;
    table->entries[ARTIFACT_SOLAR_CORE].holder = ARTIFACT_HOLDER_NONE;
    table->entries[ARTIFACT_SOLAR_CORE].waitingFor = -1;
    strncpy(table->entries[ARTIFACT_SOLAR_CORE].name, "Solar Core", 29);

    // Lunar Blade
    table->entries[ARTIFACT_LUNAR_BLADE].introduced = 1;
    table->entries[ARTIFACT_LUNAR_BLADE].holder = ARTIFACT_HOLDER_NONE;
    table->entries[ARTIFACT_LUNAR_BLADE].waitingFor = -1;
    strncpy(table->entries[ARTIFACT_LUNAR_BLADE].name, "Lunar Blade", 29);

    // Eclipse Relic — not yet in the world
    table->entries[ARTIFACT_ECLIPSE_RELIC].introduced = 0;
    table->entries[ARTIFACT_ECLIPSE_RELIC].holder = ARTIFACT_HOLDER_NONE;
    table->entries[ARTIFACT_ECLIPSE_RELIC].waitingFor = -1;
    strncpy(table->entries[ARTIFACT_ECLIPSE_RELIC].name, "Eclipse Relic", 29);
}

// ─────────────────────────────────────────────────────────────────────────────
// acquireArtifact
// ─────────────────────────────────────────────────────────────────────────────
int acquireArtifact(ArtifactTable *table, int artifactId, int holderEncoded)
{
    if (artifactId < 0 || artifactId >= ARTIFACT_COUNT)
        return 0;

    sem_wait(&table->artLock);

    ArtifactEntry &entry = table->entries[artifactId];

    if (!entry.introduced || entry.holder != ARTIFACT_HOLDER_NONE)
    {
        // Already held — record that this holder is waiting for it
        // so the deadlock detector can see the edge.
        // Find any artifact this holder already owns and mark waitingFor.
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

    // Artifact is free — acquire it
    entry.holder = holderEncoded;
    // Clear any waitingFor this holder had on other entries (they got what they wanted)
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

// ─────────────────────────────────────────────────────────────────────────────
// releaseArtifact
// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
// introduceEclipseRelic
// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
// holderHasArtifact
// ─────────────────────────────────────────────────────────────────────────────
int holderHasArtifact(ArtifactTable *table, int artifactId, int holderEncoded)
{
    if (artifactId < 0 || artifactId >= ARTIFACT_COUNT)
        return 0;

    sem_wait(&table->artLock);
    int result = (table->entries[artifactId].holder == holderEncoded) ? 1 : 0;
    sem_post(&table->artLock);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// forceReleaseArtifact — called by deadlock resolver
// ─────────────────────────────────────────────────────────────────────────────
int forceReleaseArtifact(ArtifactTable *table, int artifactId)
{
    if (artifactId < 0 || artifactId >= ARTIFACT_COUNT)
        return ARTIFACT_HOLDER_NONE;

    sem_wait(&table->artLock);

    int victim = table->entries[artifactId].holder;
    table->entries[artifactId].holder = ARTIFACT_HOLDER_NONE;
    table->entries[artifactId].waitingFor = -1;

    // Also clear waitingFor on any other entry this victim holds
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

// ─────────────────────────────────────────────────────────────────────────────
// printArtifactTable
// ─────────────────────────────────────────────────────────────────────────────
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