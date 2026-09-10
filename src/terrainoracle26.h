#ifndef TERRAINORACLE26_H
#define TERRAINORACLE26_H

#include <QString>
#include <cstdint>

// Returns the exact vanilla Java 26.2 ruined-portal anchor Y. The caller
// supplies the Java-LCG internal state immediately after template mirroring.
bool exactRuinedPortalY26(
    uint64_t worldSeed, uint64_t structureRandom,
    int location, int ySpan,
    int minX, int minZ, int maxX, int maxZ,
    bool largeBiomes, int *portalY, QString *error = nullptr);

// This is a cheap installation check. It does not start Java.
bool isExactTerrainOracle26Available(QString *error = nullptr);

// Starts the shared helper without waiting for bootstrap to finish. Search
// setup uses this to overlap Java startup with the native structure filters.
void warmUpExactTerrainOracle26();

#endif // TERRAINORACLE26_H
