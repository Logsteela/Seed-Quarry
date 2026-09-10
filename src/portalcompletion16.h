#ifndef PORTALCOMPLETION16_H
#define PORTALCOMPLETION16_H

#include "cubiomes/finders.h"

#include <stdint.h>

struct RuinedPortalCompletion16Details
{
    bool supported = false;
    bool lootSufficient = false;
    bool frameSufficient = false;
    bool approximateTerrain = false;
    int portalY = -1;
    int requiredObsidian = 0;
};

/**
 * Tests whether a Java 1.16.x Overworld ruined portal can be completed and
 * lit using its own chest. This first implementation intentionally supports
 * only normal templates portal_1, portal_6 and portal_9.
 *
 * The supplied variant and position must refer to the same structure start.
 * Surface noise must be initialized for the generator's Overworld seed.
 * When details is null, insufficient Loot returns before terrain generation;
 * a non-null details pointer computes both halves for diagnostics.
 */
bool isSelfCompletableRuinedPortal16(
    uint64_t worldSeed,
    int mc,
    Pos structurePos,
    const StructureVariant& variant,
    const Generator *generator,
    const SurfaceNoise *surfaceNoise,
    RuinedPortalCompletion16Details *details = nullptr);

/**
 * Java 26.2 counterpart of the 1.16 test. Loot, template selection,
 * transformation and crying-obsidian replacement are reconstructed from the
 * exact seed. Cubiomes has no post-1.17 block terrain generator, so the portal
 * Y used by the replacement test comes from its modern surface approximation.
 */
bool isSelfCompletableRuinedPortal26(
    uint64_t worldSeed,
    Pos structurePos,
    const StructureVariant& variant,
    const Generator *generator,
    RuinedPortalCompletion16Details *details = nullptr);

#endif
