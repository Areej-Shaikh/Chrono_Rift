#ifndef ARTIFACT_TABLE_H
#define ARTIFACT_TABLE_H

// ─────────────────────────────────────────────────────────────────────────────
// Artifact IDs — fixed indices into ArtifactTable::entries[]
// ─────────────────────────────────────────────────────────────────────────────
#define ARTIFACT_SOLAR_CORE 0
#define ARTIFACT_LUNAR_BLADE 1
#define ARTIFACT_ECLIPSE_RELIC 2 // dynamic — only valid when introduced == 1
#define ARTIFACT_COUNT 3

// ─────────────────────────────────────────────────────────────────────────────
// Who can hold an artifact
// ─────────────────────────────────────────────────────────────────────────────
#define ARTIFACT_HOLDER_NONE -1 // free
// Positive values: player id (0–3)
// Negative values below -1: enemy id encoded as -(enemyId + 2)
//   e.g. enemy 0 → -2, enemy 1 → -3, ...
// Use the helpers below to encode/decode.

inline int encodeEnemyHolder(int enemyId) { return -(enemyId + 2); }
inline int decodeEnemyHolder(int encoded) { return -(encoded + 2); }
inline int isEnemyHolder(int h) { return h <= -2; }
inline int isPlayerHolder(int h) { return h >= 0; }

// ─────────────────────────────────────────────────────────────────────────────
// ArtifactEntry — one row of the global resource table (spec Section 7)
// ─────────────────────────────────────────────────────────────────────────────
struct ArtifactEntry
{
    int introduced; // 1 = exists in game world, 0 = not yet
    int holder;     // ARTIFACT_HOLDER_NONE (-1) = free, else encoded holder
    int waitingFor; // if this entity holds THIS artifact and is waiting for
                    // another, store that other artifact's id here (-1 = none)
                    // used by the deadlock detector
    char name[30];
};

// ─────────────────────────────────────────────────────────────────────────────
// ArtifactTable — lives inside SharedState
// Protected by its own mutex (artLock) so artifact operations don't hold the
// main stateLock longer than necessary.
// ─────────────────────────────────────────────────────────────────────────────
#include <semaphore.h>

struct ArtifactTable
{
    sem_t artLock; // mutex: 1 = unlocked
    ArtifactEntry entries[ARTIFACT_COUNT];
};

#endif // ARTIFACT_TABLE_H