#ifndef ARTIFACT_MANAGER_H
#define ARTIFACT_MANAGER_H

#include "artifact_table.h"


void initArtifactTable(ArtifactTable *table);


int acquireArtifact(ArtifactTable *table, int artifactId, int holderEncoded);


int releaseArtifact(ArtifactTable *table, int artifactId, int holderEncoded);


void introduceEclipseRelic(ArtifactTable *table);


int holderHasArtifact(ArtifactTable *table, int artifactId, int holderEncoded);


int forceReleaseArtifact(ArtifactTable *table, int artifactId);


void printArtifactTable(ArtifactTable *table);

#endif 