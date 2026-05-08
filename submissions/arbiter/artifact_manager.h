#ifndef ARTIFACT_MANAGER_H
#define ARTIFACT_MANAGER_H

#include "artifact_table.h"

// ─────────────────────────────────────────────────────────────────────────────
// Call once at game start to initialise the table.
// Solar Core and Lunar Blade start as free but introduced.
// Eclipse Relic starts as not-introduced (introduced = 0).
// ─────────────────────────────────────────────────────────────────────────────
void initArtifactTable(ArtifactTable *table);

// ─────────────────────────────────────────────────────────────────────────────
// Try to lock an artifact for a holder.
// Returns 1 on success (artifact was free), 0 if already held.
// Also sets waitingFor on the holder's current artifact if they hold one,
// so the deadlock detector can see the circular-wait edge.
// ─────────────────────────────────────────────────────────────────────────────
int acquireArtifact(ArtifactTable *table, int artifactId, int holderEncoded);

// ─────────────────────────────────────────────────────────────────────────────
// Release an artifact held by holderEncoded.
// Clears waitingFor on all entries that were waiting for this artifact.
// Returns 1 if the release happened, 0 if the holder didn't actually hold it.
// ─────────────────────────────────────────────────────────────────────────────
int releaseArtifact(ArtifactTable *table, int artifactId, int holderEncoded);

// ─────────────────────────────────────────────────────────────────────────────
// Introduce the Eclipse Relic dynamically at runtime (spec Section 7).
// Does nothing if already introduced.
// ─────────────────────────────────────────────────────────────────────────────
void introduceEclipseRelic(ArtifactTable *table);

// ─────────────────────────────────────────────────────────────────────────────
// Returns 1 if the given holder currently holds the given artifact.
// ─────────────────────────────────────────────────────────────────────────────
int holderHasArtifact(ArtifactTable *table, int artifactId, int holderEncoded);

// ─────────────────────────────────────────────────────────────────────────────
// Force-release: the deadlock resolver calls this to take an artifact away
// from whoever holds it.  Returns the encoded holder that lost it, or
// ARTIFACT_HOLDER_NONE if the artifact was already free.
// ─────────────────────────────────────────────────────────────────────────────
int forceReleaseArtifact(ArtifactTable *table, int artifactId);

// ─────────────────────────────────────────────────────────────────────────────
// Print the current table state to stdout (for debugging / demo).
// ─────────────────────────────────────────────────────────────────────────────
void printArtifactTable(ArtifactTable *table);

#endif // ARTIFACT_MANAGER_H