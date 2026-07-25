#ifndef LOOT_H_
#define LOOT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum DesertPyramidLootItem
{
    /* Keep the original desert-pyramid values stable for saved GUI rules. */
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

    /* Buried treasure. */
    DP_LOOT_HEART_OF_THE_SEA,
    DP_LOOT_TNT,
    DP_LOOT_PRISMARINE_CRYSTALS,
    DP_LOOT_LEATHER_CHESTPLATE,
    DP_LOOT_IRON_SWORD,
    DP_LOOT_COOKED_COD,
    DP_LOOT_COOKED_SALMON,

    /* Ruined portal. */
    DP_LOOT_OBSIDIAN,
    DP_LOOT_FLINT,
    DP_LOOT_IRON_NUGGET,
    DP_LOOT_FLINT_AND_STEEL,
    DP_LOOT_FIRE_CHARGE,
    DP_LOOT_GOLD_NUGGET,
    DP_LOOT_GOLDEN_SWORD,
    DP_LOOT_GOLDEN_AXE,
    DP_LOOT_GOLDEN_HOE,
    DP_LOOT_GOLDEN_SHOVEL,
    DP_LOOT_GOLDEN_PICKAXE,
    DP_LOOT_GOLDEN_BOOTS,
    DP_LOOT_GOLDEN_CHESTPLATE,
    DP_LOOT_GOLDEN_HELMET,
    DP_LOOT_GOLDEN_LEGGINGS,
    DP_LOOT_GLISTERING_MELON_SLICE,
    DP_LOOT_LIGHT_WEIGHTED_PRESSURE_PLATE,
    DP_LOOT_GOLDEN_CARROT,
    DP_LOOT_CLOCK,
    DP_LOOT_GOLD_BLOCK,
    DP_LOOT_BELL,

    /* Shipwreck. */
    DP_LOOT_FILLED_MAP,
    DP_LOOT_COMPASS,
    DP_LOOT_MAP,
    DP_LOOT_PAPER,
    DP_LOOT_FEATHER,
    DP_LOOT_BOOK,
    DP_LOOT_POTATO,
    DP_LOOT_POISONOUS_POTATO,
    DP_LOOT_CARROT,
    DP_LOOT_WHEAT,
    DP_LOOT_SUSPICIOUS_STEW,
    DP_LOOT_COAL,
    DP_LOOT_PUMPKIN,
    DP_LOOT_BAMBOO,
    DP_LOOT_LEATHER_HELMET,
    DP_LOOT_LEATHER_LEGGINGS,
    DP_LOOT_LEATHER_BOOTS,
    DP_LOOT_EXPERIENCE_BOTTLE,
    DP_LOOT_LAPIS_LAZULI,

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

/* The original type name remains source-compatible. */
typedef DesertPyramidLoot StructureLoot;

enum ShipwreckLootChest
{
    SHIPWRECK_CHEST_SUPPLY,
    SHIPWRECK_CHEST_MAP,
    SHIPWRECK_CHEST_TREASURE,
    SHIPWRECK_CHEST_COUNT
};

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

/**
 * Java 1.16.x structure loot implemented from the pinned SeedFinding/MineMap
 * sources. Shipwreck output is indexed by enum ShipwreckLootChest; present[i]
 * is zero when that ship template has no chest of that type.
 */
int getBuriedTreasureLoot16(StructureLoot *out, uint64_t worldSeed,
                           int chunkX, int chunkZ);
int getRuinedPortalLoot16(StructureLoot *out, uint64_t worldSeed,
                         int chunkX, int chunkZ);
int getShipwreckLoot16(StructureLoot out[SHIPWRECK_CHEST_COUNT],
                       uint8_t present[SHIPWRECK_CHEST_COUNT],
                       uint64_t worldSeed, int chunkX, int chunkZ,
                       int isBeached);

const char *desertPyramidLootItemName(int item);
const char *desertPyramidEnchantmentName(int enchantment);
int desertPyramidEnchantmentMaxLevel(int enchantment);

const char *structureLootItemName(int item);
int structureLootItemAvailable(int structureType, int item);

#ifdef __cplusplus
}
#endif

#endif
