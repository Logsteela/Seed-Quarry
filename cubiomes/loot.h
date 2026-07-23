#ifndef LOOT_H_
#define LOOT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum DesertPyramidLootItem
{
    DP_LOOT_DIAMOND,
    DP_LOOT_IRON_INGOT,
    DP_LOOT_GOLD_INGOT,
    DP_LOOT_EMERALD,
    DP_LOOT_BONE,
    DP_LOOT_SPIDER_EYE,
    DP_LOOT_ROTTEN_FLESH,
    DP_LOOT_SADDLE,
    DP_LOOT_IRON_HORSE_ARMOR,
    DP_LOOT_GOLDEN_HORSE_ARMOR,
    DP_LOOT_DIAMOND_HORSE_ARMOR,
    DP_LOOT_ENCHANTED_BOOK,
    DP_LOOT_GOLDEN_APPLE,
    DP_LOOT_ENCHANTED_GOLDEN_APPLE,
    DP_LOOT_GUNPOWDER,
    DP_LOOT_STRING,
    DP_LOOT_SAND,
    DP_LOOT_ITEM_COUNT
};

typedef struct DesertPyramidLoot
{
    uint16_t count[DP_LOOT_ITEM_COUNT];
} DesertPyramidLoot;

/**
 * Calculates the combined, non-indexed contents of one desert-pyramid chest
 * for Minecraft Java 1.16.x.
 *
 * chestIndex is the Vanilla RNG order in the structure (0..3), not a physical
 * direction. Returns zero when chestIndex, coordinates, or the output pointer
 * are invalid.
 */
int getDesertPyramidLoot16(DesertPyramidLoot *out, uint64_t worldSeed,
                           int chunkX, int chunkZ, int chestIndex);

const char *desertPyramidLootItemName(int item);

#ifdef __cplusplus
}
#endif

#endif
