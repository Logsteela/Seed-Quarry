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

enum DesertPyramidEnchantment
{
    DP_ENCH_PROTECTION,
    DP_ENCH_FIRE_PROTECTION,
    DP_ENCH_FEATHER_FALLING,
    DP_ENCH_BLAST_PROTECTION,
    DP_ENCH_PROJECTILE_PROTECTION,
    DP_ENCH_RESPIRATION,
    DP_ENCH_AQUA_AFFINITY,
    DP_ENCH_THORNS,
    DP_ENCH_DEPTH_STRIDER,
    DP_ENCH_FROST_WALKER,
    DP_ENCH_BINDING_CURSE,
    DP_ENCH_SHARPNESS,
    DP_ENCH_SMITE,
    DP_ENCH_BANE_OF_ARTHROPODS,
    DP_ENCH_KNOCKBACK,
    DP_ENCH_FIRE_ASPECT,
    DP_ENCH_LOOTING,
    DP_ENCH_SWEEPING,
    DP_ENCH_EFFICIENCY,
    DP_ENCH_SILK_TOUCH,
    DP_ENCH_UNBREAKING,
    DP_ENCH_FORTUNE,
    DP_ENCH_POWER,
    DP_ENCH_PUNCH,
    DP_ENCH_FLAME,
    DP_ENCH_INFINITY,
    DP_ENCH_LUCK_OF_THE_SEA,
    DP_ENCH_LURE,
    DP_ENCH_LOYALTY,
    DP_ENCH_IMPALING,
    DP_ENCH_RIPTIDE,
    DP_ENCH_CHANNELING,
    DP_ENCH_MULTISHOT,
    DP_ENCH_QUICK_CHARGE,
    DP_ENCH_PIERCING,
    DP_ENCH_MENDING,
    DP_ENCH_VANISHING_CURSE,
    DP_ENCH_COUNT
};

#define DP_ENCH_MAX_LEVEL 5

typedef struct DesertPyramidLoot
{
    uint16_t count[DP_LOOT_ITEM_COUNT];
    /* [enchantment][level], with valid levels in the range 1..5. */
    uint16_t enchantedBook[DP_ENCH_COUNT][DP_ENCH_MAX_LEVEL + 1];
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
const char *desertPyramidEnchantmentName(int enchantment);
int desertPyramidEnchantmentMaxLevel(int enchantment);

#ifdef __cplusplus
}
#endif

#endif
