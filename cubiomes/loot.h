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

    /*
     * Village (Java 1.16.1).
     *
     * New values must only ever be appended here. The numeric values above
     * are stored in Seed Quarry condition files and therefore form a
     * compatibility boundary.
     */
    DP_LOOT_BREAD,
    DP_LOOT_IRON_HELMET,
    DP_LOOT_PORKCHOP,
    DP_LOOT_BEEF,
    DP_LOOT_MUTTON,
    DP_LOOT_STICK,
    DP_LOOT_CLAY_BALL,
    DP_LOOT_GREEN_DYE,
    DP_LOOT_CACTUS,
    DP_LOOT_COD,
    DP_LOOT_SALMON,
    DP_LOOT_WATER_BUCKET,
    DP_LOOT_BARREL,
    DP_LOOT_WHEAT_SEEDS,
    DP_LOOT_ARROW,
    DP_LOOT_EGG,
    DP_LOOT_FLOWER_POT,
    DP_LOOT_STONE,
    DP_LOOT_STONE_BRICKS,
    DP_LOOT_YELLOW_DYE,
    DP_LOOT_SMOOTH_STONE,
    DP_LOOT_DANDELION,
    DP_LOOT_POPPY,
    DP_LOOT_APPLE,
    DP_LOOT_OAK_SAPLING,
    DP_LOOT_GRASS,
    DP_LOOT_TALL_GRASS,
    DP_LOOT_ACACIA_SAPLING,
    DP_LOOT_TORCH,
    DP_LOOT_BUCKET,
    DP_LOOT_WHITE_WOOL,
    DP_LOOT_BLACK_WOOL,
    DP_LOOT_GRAY_WOOL,
    DP_LOOT_BROWN_WOOL,
    DP_LOOT_LIGHT_GRAY_WOOL,
    DP_LOOT_SHEARS,
    DP_LOOT_BLUE_ICE,
    DP_LOOT_SNOW_BLOCK,
    DP_LOOT_BEETROOT_SEEDS,
    DP_LOOT_BEETROOT_SOUP,
    DP_LOOT_FURNACE,
    DP_LOOT_SNOWBALL,
    DP_LOOT_FERN,
    DP_LOOT_LARGE_FERN,
    DP_LOOT_SWEET_BERRIES,
    DP_LOOT_PUMPKIN_SEEDS,
    DP_LOOT_PUMPKIN_PIE,
    DP_LOOT_SPRUCE_SAPLING,
    DP_LOOT_SPRUCE_SIGN,
    DP_LOOT_SPRUCE_LOG,
    DP_LOOT_LEATHER,
    DP_LOOT_REDSTONE,
    DP_LOOT_IRON_PICKAXE,
    DP_LOOT_IRON_SHOVEL,
    DP_LOOT_IRON_CHESTPLATE,
    DP_LOOT_IRON_LEGGINGS,
    DP_LOOT_IRON_BOOTS,

    /* Bastion remnant (Java 1.16.1). */
    DP_LOOT_LODESTONE,
    DP_LOOT_CROSSBOW,
    DP_LOOT_SPECTRAL_ARROW,
    DP_LOOT_GILDED_BLACKSTONE,
    DP_LOOT_CRYING_OBSIDIAN,
    DP_LOOT_DIAMOND_SHOVEL,
    DP_LOOT_NETHERITE_SCRAP,
    DP_LOOT_ANCIENT_DEBRIS,
    DP_LOOT_GLOWSTONE,
    DP_LOOT_SOUL_SAND,
    DP_LOOT_CRIMSON_NYLIUM,
    DP_LOOT_COOKED_PORKCHOP,
    DP_LOOT_CRIMSON_FUNGUS,
    DP_LOOT_CRIMSON_ROOTS,
    DP_LOOT_PIGLIN_BANNER_PATTERN,
    DP_LOOT_MUSIC_DISC_PIGSTEP,
    DP_LOOT_CHAIN,
    DP_LOOT_MAGMA_CREAM,
    DP_LOOT_BONE_BLOCK,
    DP_LOOT_NETHERITE_INGOT,
    DP_LOOT_DIAMOND_SWORD,
    DP_LOOT_DIAMOND_CHESTPLATE,
    DP_LOOT_DIAMOND_HELMET,
    DP_LOOT_DIAMOND_LEGGINGS,
    DP_LOOT_DIAMOND_BOOTS,
    DP_LOOT_QUARTZ,
    DP_LOOT_DEAD_BUSH,

    /*
     * Seed Quarry virtual item. This is not present in a Minecraft Loot table;
     * it counts one for every generated randomizable container and enables
     * position-only/chest-count filters without guessing the chest contents.
     * Keep it appended so existing serialized item numbers remain stable.
     */
    DP_LOOT_ANY_CONTAINER,

    /* Bastion remnant additions used by Java 1.16.2 through 1.16.5. */
    DP_LOOT_DIAMOND_PICKAXE,
    DP_LOOT_IRON_BLOCK,

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
    /* Appended to keep all pre-existing saved enchantment ids stable. */
    DP_ENCH_SOUL_SPEED,
    DP_ENCH_COUNT
};

#define DP_ENCH_MAX_LEVEL 5
#define STRUCTURE_LOOT_MAX_ENCHANTMENTS 16

typedef struct StructureLootEnchantment
{
    uint16_t item;
    uint8_t enchantment;
    uint8_t level;
    uint16_t count;
} StructureLootEnchantment;

typedef struct DesertPyramidLoot
{
    uint16_t count[DP_LOOT_ITEM_COUNT];
    /* [enchantment][level], with valid levels in the range 1..5. */
    uint16_t enchantedBook[DP_ENCH_COUNT][DP_ENCH_MAX_LEVEL + 1];
    /*
     * Enchantments attached to books and equipment. Keeping this sparse
     * avoids a large item x enchantment x level matrix in every chest.
     */
    uint8_t enchantmentCount;
    StructureLootEnchantment enchantments[
        STRUCTURE_LOOT_MAX_ENCHANTMENTS];
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
 * Loot table ids are deliberately independent of cubiomes StructureType:
 * villages and bastions each contain several different chest tables.
 */
enum StructureLootTable16
{
    LOOT_TABLE16_VILLAGE_ARMORER,
    LOOT_TABLE16_VILLAGE_BUTCHER,
    LOOT_TABLE16_VILLAGE_CARTOGRAPHER,
    LOOT_TABLE16_VILLAGE_DESERT_HOUSE,
    LOOT_TABLE16_VILLAGE_FISHER,
    LOOT_TABLE16_VILLAGE_FLETCHER,
    LOOT_TABLE16_VILLAGE_MASON,
    LOOT_TABLE16_VILLAGE_PLAINS_HOUSE,
    LOOT_TABLE16_VILLAGE_SAVANNA_HOUSE,
    LOOT_TABLE16_VILLAGE_SHEPHERD,
    LOOT_TABLE16_VILLAGE_SNOWY_HOUSE,
    LOOT_TABLE16_VILLAGE_TAIGA_HOUSE,
    LOOT_TABLE16_VILLAGE_TANNERY,
    LOOT_TABLE16_VILLAGE_TEMPLE,
    LOOT_TABLE16_VILLAGE_TOOLSMITH,
    LOOT_TABLE16_VILLAGE_WEAPONSMITH,
    LOOT_TABLE16_BASTION_BRIDGE,
    LOOT_TABLE16_BASTION_HOGLIN_STABLE,
    LOOT_TABLE16_BASTION_OTHER,
    LOOT_TABLE16_BASTION_TREASURE,
    LOOT_TABLE16_BASTION_BRIDGE_1_16_5,
    LOOT_TABLE16_BASTION_HOGLIN_STABLE_1_16_5,
    LOOT_TABLE16_BASTION_OTHER_1_16_5,
    LOOT_TABLE16_BASTION_TREASURE_1_16_5,
    LOOT_TABLE16_COUNT
};

/*
 * Bastion chest Loot changed in Java 1.16.2. Seed Atlas exposes only a
 * single "1.16" world-generation choice, so Loot conditions carry this
 * minor-version profile explicitly.
 */
enum BastionLootProfile16
{
    BASTION_LOOT_PROFILE_1_16_1,
    BASTION_LOOT_PROFILE_1_16_2_TO_1_16_5,
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

/**
 * Generates the combined (non-slot-indexed) contents of one Java 1.16.1
 * village or bastion loot table from the LootTableSeed stored on that chest.
 *
 * lootTableSeed is the 64-bit NBT LootTableSeed value, not the world seed.
 * Returns zero for a null output pointer or an invalid table id.
 */
int generateStructureLootTable16(StructureLoot *out, int table,
                                 uint64_t lootTableSeed);
const char *structureLootTable16Name(int table);

const char *desertPyramidLootItemName(int item);
const char *desertPyramidEnchantmentName(int enchantment);
int desertPyramidEnchantmentMaxLevel(int enchantment);

const char *structureLootItemName(int item);
int structureLootItemAvailable(int structureType, int item,
                               int bastionLootProfile);
int structureLootItemCanBeEnchanted(int structureType, int item,
                                    int bastionLootProfile);
int structureLootEnchantmentAvailable(int structureType, int item,
                                      int enchantment,
                                      int bastionLootProfile);

#ifdef __cplusplus
}
#endif

#endif
