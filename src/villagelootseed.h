#ifndef VILLAGELOOTSEED_H
#define VILLAGELOOTSEED_H

#include "villagestructure.h"

#include <QString>
#include <QVector>

#include <stdint.h>

enum VillageLootSeedQuality16
{
    VILLAGE_LOOT_SEED_EXACT,
    VILLAGE_LOOT_SEED_UNRESOLVED_FEATURE,
    VILLAGE_LOOT_SEED_UNRESOLVED_OVERLAP,
};

enum VillageLootTerrainMode16
{
    VILLAGE_LOOT_TERRAIN_LIGHT,
    VILLAGE_LOOT_TERRAIN_DETAILED,
};

enum VillageLootSeedUnresolvedReason16
{
    VILLAGE_LOOT_UNRESOLVED_NONE,
    VILLAGE_LOOT_UNRESOLVED_CROSS_CHUNK,
    VILLAGE_LOOT_UNRESOLVED_PATH_STATE,
    VILLAGE_LOOT_UNRESOLVED_TEMPLATE_STATE,
    VILLAGE_LOOT_UNRESOLVED_SURFACE_MISSING,
    VILLAGE_LOOT_UNRESOLVED_WATER_LEVEL,
    VILLAGE_LOOT_UNRESOLVED_DEEP_TERRAIN,
    VILLAGE_LOOT_UNRESOLVED_UNKNOWN_FEATURE,
};

const char *villageLootUnresolvedReasonName16(int reason);

/**
 * A loot-table-bearing Village container and the seed assigned while its
 * structure piece is placed in Java 1.16.1.
 *
 * lootTableSeed is meaningful only when quality is
 * VILLAGE_LOOT_SEED_EXACT. Position and piece metadata remain exact for the
 * unresolved qualities.
 */
struct VillageLootChestSeed16
{
    VillageContainer16 container;
    uint64_t lootTableSeed = 0;
    int quality = VILLAGE_LOOT_SEED_EXACT;
    int unresolvedFeatureIndex = -1;
    int unresolvedReason = VILLAGE_LOOT_UNRESOLVED_NONE;
    Pos3 unresolvedPos = {};

    bool isExact() const
    {
        return quality == VILLAGE_LOOT_SEED_EXACT;
    }
};

/**
 * Assign Village LootTableSeed values for the common single-start case.
 *
 * The routine walks pieces in their saved order independently for every
 * chest chunk. It advances through containers without a LootTable as vanilla
 * does. Supported feature elements are reproduced against the layout's
 * compact block-state model. A feature whose relevant world state is not
 * known makes only the later containers in that chunk unresolved.
 *
 * Set anotherVillageMayReferenceAChestChunk when another Village start may
 * be present in any output chest's chunk. In that case positions are still
 * returned, but every seed is marked UNRESOLVED_OVERLAP.
 *
 * Only containers with a non-empty lootTable are returned.
 */
bool assignVillageLootSeedsSingleStart16(
    QVector<VillageLootChestSeed16> *out,
    const VillageLayout16& layout, uint64_t worldSeed,
    bool anotherVillageMayReferenceAChestChunk,
    QString *error = nullptr);

/**
 * Per-chunk overlap form. Each Pos stores chunk coordinates in x/z. Only
 * loot containers in one of those chunks are marked UNRESOLVED_OVERLAP.
 */
bool assignVillageLootSeedsSingleStart16(
    QVector<VillageLootChestSeed16> *out,
    const VillageLayout16& layout, uint64_t worldSeed,
    const QVector<Pos>& overlappingChestChunks,
    QString *error = nullptr);

/**
 * Detailed terrain retries only chunks which the light compact model left
 * unresolved. It reconstructs Java 1.16.1 base terrain, surface layers, and
 * land cave/ravine carvers; unsupported terrain remains explicitly unknown.
 */
bool assignVillageLootSeedsSingleStart16(
    QVector<VillageLootChestSeed16> *out,
    const VillageLayout16& layout, uint64_t worldSeed,
    const QVector<Pos>& overlappingChestChunks,
    int terrainMode, QString *error = nullptr);

#endif
