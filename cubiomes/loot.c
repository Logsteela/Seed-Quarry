#include "loot.h"

#include "finders.h"
#include "rng.h"

#include <string.h>

/*
 * Minecraft Java 1.16 desert-pyramid loot behavior. The reference pipeline
 * and table are from SeedFinding mc_core_java 1.192.1 and mc_feature_java
 * 1.171.1, both licensed under the MIT License. See THIRD_PARTY_NOTICES.md.
 */

enum
{
    DP_DECORATION_SALT_16 = 40003,
    BURIED_DECORATION_SALT_16 = 30001,
    PORTAL_DECORATION_SALT_16 = 40005,
    SHIPWRECK_DECORATION_SALT_16 = 40006,
    DP_PRIMARY_WEIGHT = 232,
    /* Soul Speed is not discoverable and is excluded from the old tables. */
    DP_BOOK_ENCHANTMENTS_16 = DP_ENCH_SOUL_SPEED,
    DP_WORLD_BORDER_CHUNKS = 1875000,
};

static const uint8_t DP_BOOK_MAX_LEVEL_16[DP_BOOK_ENCHANTMENTS_16] = {
    4, 4, 4, 4, 4, 3, 1, 3, 3, 2, 1,
    5, 5, 5, 2, 2, 3, 3, 5, 1, 3, 3,
    5, 2, 1, 1, 3, 3, 3, 5, 3, 1, 1,
    3, 4, 1, 1,
};

enum LootFunction16
{
    LOOT_FUNCTION_NONE,
    LOOT_FUNCTION_ENCHANT_RANDOMLY,
    LOOT_FUNCTION_STEW_EFFECT,
};

typedef struct LootEntry16
{
    uint8_t item;
    uint16_t weight;
    uint8_t minCount;
    uint8_t maxCount;
    uint8_t function;
} LootEntry16;

enum LootTableFlags16
{
    LT16_NONE = 0,
    LT16_EMPTY = 1 << 0,
    LT16_DAMAGE = 1 << 1,
    LT16_ENCHANT = 1 << 2,
    LT16_SOUL_SPEED = 1 << 3,
};

typedef struct LootTableEntry16
{
    int16_t item;
    uint16_t weight;
    uint8_t minCount;
    uint8_t maxCount;
    uint8_t flags;
} LootTableEntry16;

typedef struct LootTablePool16
{
    uint8_t minRolls;
    uint8_t maxRolls;
    uint16_t firstEntry;
    uint16_t entryCount;
} LootTablePool16;

typedef struct LootTableDefinition16
{
    uint8_t firstPool;
    uint8_t poolCount;
} LootTableDefinition16;

#include "loot_tables_1_16_1.inc"
#include "loot_tables_bastion_1_16_5.inc"

typedef struct ModernEnchantment
{
    uint16_t item;
    uint8_t enchantment;
    uint8_t maxLevel;
} ModernEnchantment;

#include "loot_tables_26_2.inc"

static int modernEnchantmentLevel(int item, int enchantment)
{
    for (unsigned i = 0; i < sizeof(MODERN_ENCHANTMENTS_26_2) /
            sizeof(MODERN_ENCHANTMENTS_26_2[0]); i++)
    {
        const ModernEnchantment *e = MODERN_ENCHANTMENTS_26_2 + i;
        if (e->item == item && e->enchantment == enchantment)
            return e->maxLevel;
    }
    return 0;
}

static int selectModernEnchantment(uint64_t *rng, int item, int *level)
{
    const ModernEnchantment *choices[DP_ENCH_COUNT];
    int count = 0;
    for (unsigned i = 0; i < sizeof(MODERN_ENCHANTMENTS_26_2) /
            sizeof(MODERN_ENCHANTMENTS_26_2[0]); i++)
    {
        if (MODERN_ENCHANTMENTS_26_2[i].item == item)
            choices[count++] = MODERN_ENCHANTMENTS_26_2 + i;
    }
    if (!count)
        return -1;
    const ModernEnchantment *e = choices[nextInt(rng, count)];
    *level = e->maxLevel == 1 ? 1 : 1 + nextInt(rng, e->maxLevel);
    return e->enchantment;
}

static int resolveStructureLootTable16(
    int table, const LootTableDefinition16 **definition,
    const LootTablePool16 **pools,
    const LootTableEntry16 **entries, const char **name)
{
    if (table >= LOOT_TABLE26_BASTION_BRIDGE &&
        table <= LOOT_TABLE26_RUINED_PORTAL)
    {
        int local = table - LOOT_TABLE26_BASTION_BRIDGE;
        if (definition) *definition = MODERN_LOOT_TABLES_26_2 + local;
        if (pools) *pools = MODERN_LOOT_POOLS_26_2;
        if (entries) *entries = MODERN_LOOT_ENTRIES_26_2;
        if (name) *name = MODERN_LOOT_TABLE_NAMES_26_2[local];
        return 1;
    }
    if (table >= LOOT_TABLE16_BASTION_BRIDGE_1_16_5 &&
        table <= LOOT_TABLE16_BASTION_TREASURE_1_16_5)
    {
        int local = table - LOOT_TABLE16_BASTION_BRIDGE_1_16_5;
        if (definition)
            *definition = BASTION_LOOT_TABLES_1_16_5 + local;
        if (pools)
            *pools = BASTION_LOOT_POOLS_1_16_5;
        if (entries)
            *entries = BASTION_LOOT_ENTRIES_1_16_5;
        if (name)
            *name = BASTION_LOOT_TABLE_NAMES_1_16_5[local];
        return 1;
    }
    if (table < 0 || table > LOOT_TABLE16_BASTION_TREASURE)
        return 0;
    if (definition)
        *definition = STRUCTURE_LOOT_TABLES_16 + table;
    if (pools)
        *pools = STRUCTURE_LOOT_POOLS_16;
    if (entries)
        *entries = STRUCTURE_LOOT_ENTRIES_16;
    if (name)
        *name = STRUCTURE_LOOT_TABLE_NAMES_16[table];
    return 1;
}

typedef struct ShipTemplate16
{
    uint8_t sizeZ;
    uint8_t count;
    uint8_t kind[SHIPWRECK_CHEST_COUNT];
    uint8_t x[SHIPWRECK_CHEST_COUNT];
    uint8_t z[SHIPWRECK_CHEST_COUNT];
} ShipTemplate16;

enum ShipTemplateId16
{
    SHIP_WITH_MAST,
    SHIP_UPSIDEDOWN_FULL,
    SHIP_UPSIDEDOWN_FRONT,
    SHIP_UPSIDEDOWN_BACK,
    SHIP_SIDEWAYS_FULL,
    SHIP_SIDEWAYS_FRONT,
    SHIP_SIDEWAYS_BACK,
    SHIP_RIGHTSIDEUP_FULL,
    SHIP_RIGHTSIDEUP_FRONT,
    SHIP_RIGHTSIDEUP_BACK,
};

static const ShipTemplate16 SHIP_TEMPLATES_16[] = {
    {28, 3, {SHIPWRECK_CHEST_SUPPLY, SHIPWRECK_CHEST_MAP,
             SHIPWRECK_CHEST_TREASURE},
            {4, 5, 6}, {9, 18, 24}},
    {28, 3, {SHIPWRECK_CHEST_TREASURE, SHIPWRECK_CHEST_MAP,
             SHIPWRECK_CHEST_SUPPLY},
            {2, 3, 4}, {24, 17, 8}},
    {22, 2, {SHIPWRECK_CHEST_MAP, SHIPWRECK_CHEST_SUPPLY},
            {3, 4, 0}, {17, 8, 0}},
    {16, 2, {SHIPWRECK_CHEST_TREASURE, SHIPWRECK_CHEST_MAP},
            {2, 3, 0}, {12, 5, 0}},
    {28, 3, {SHIPWRECK_CHEST_TREASURE, SHIPWRECK_CHEST_SUPPLY,
             SHIPWRECK_CHEST_MAP},
            {3, 5, 6}, {24, 8, 19}},
    {24, 1, {SHIPWRECK_CHEST_SUPPLY},
            {5, 0, 0}, {8, 0, 0}},
    {17, 2, {SHIPWRECK_CHEST_TREASURE, SHIPWRECK_CHEST_MAP},
            {3, 6, 0}, {13, 8, 0}},
    {28, 3, {SHIPWRECK_CHEST_SUPPLY, SHIPWRECK_CHEST_MAP,
             SHIPWRECK_CHEST_TREASURE},
            {4, 5, 6}, {8, 18, 24}},
    {24, 1, {SHIPWRECK_CHEST_SUPPLY},
            {4, 0, 0}, {8, 0, 0}},
    {16, 2, {SHIPWRECK_CHEST_MAP, SHIPWRECK_CHEST_TREASURE},
            {5, 6, 0}, {6, 12, 0}},
};

static const uint8_t SHIP_OCEAN_TYPES_16[] = {
    SHIP_WITH_MAST,
    SHIP_UPSIDEDOWN_FULL,
    SHIP_UPSIDEDOWN_FRONT,
    SHIP_UPSIDEDOWN_BACK,
    SHIP_SIDEWAYS_FULL,
    SHIP_SIDEWAYS_FRONT,
    SHIP_SIDEWAYS_BACK,
    SHIP_RIGHTSIDEUP_FULL,
    SHIP_RIGHTSIDEUP_FRONT,
    SHIP_RIGHTSIDEUP_BACK,
    SHIP_WITH_MAST,
    SHIP_UPSIDEDOWN_FULL,
    SHIP_UPSIDEDOWN_FRONT,
    SHIP_UPSIDEDOWN_BACK,
    SHIP_SIDEWAYS_FULL,
    SHIP_SIDEWAYS_FRONT,
    SHIP_SIDEWAYS_BACK,
    SHIP_RIGHTSIDEUP_FULL,
    SHIP_RIGHTSIDEUP_FRONT,
    SHIP_RIGHTSIDEUP_BACK,
};

static const uint8_t SHIP_BEACHED_TYPES_16[] = {
    SHIP_WITH_MAST,
    SHIP_SIDEWAYS_FULL,
    SHIP_SIDEWAYS_FRONT,
    SHIP_SIDEWAYS_BACK,
    SHIP_RIGHTSIDEUP_FULL,
    SHIP_RIGHTSIDEUP_FRONT,
    SHIP_RIGHTSIDEUP_BACK,
    SHIP_WITH_MAST,
    SHIP_RIGHTSIDEUP_FULL,
    SHIP_RIGHTSIDEUP_FRONT,
    SHIP_RIGHTSIDEUP_BACK,
};

static int lootUniform(uint64_t *rng, int low, int high)
{
    return low >= high ? low : low + nextInt(rng, high - low + 1);
}

static const uint8_t *applicableEnchantments16(int item, int *count)
{
    static const uint8_t sword[] = {
        DP_ENCH_SHARPNESS, DP_ENCH_SMITE, DP_ENCH_BANE_OF_ARTHROPODS,
        DP_ENCH_KNOCKBACK, DP_ENCH_FIRE_ASPECT, DP_ENCH_LOOTING,
        DP_ENCH_SWEEPING, DP_ENCH_UNBREAKING, DP_ENCH_MENDING,
        DP_ENCH_VANISHING_CURSE,
    };
    static const uint8_t axe[] = {
        DP_ENCH_SHARPNESS, DP_ENCH_SMITE, DP_ENCH_BANE_OF_ARTHROPODS,
        DP_ENCH_EFFICIENCY, DP_ENCH_SILK_TOUCH, DP_ENCH_UNBREAKING,
        DP_ENCH_FORTUNE, DP_ENCH_MENDING, DP_ENCH_VANISHING_CURSE,
    };
    static const uint8_t digger[] = {
        DP_ENCH_EFFICIENCY, DP_ENCH_SILK_TOUCH, DP_ENCH_UNBREAKING,
        DP_ENCH_FORTUNE, DP_ENCH_MENDING, DP_ENCH_VANISHING_CURSE,
    };
    static const uint8_t boots[] = {
        DP_ENCH_PROTECTION, DP_ENCH_FIRE_PROTECTION,
        DP_ENCH_FEATHER_FALLING, DP_ENCH_BLAST_PROTECTION,
        DP_ENCH_PROJECTILE_PROTECTION, DP_ENCH_DEPTH_STRIDER,
        DP_ENCH_FROST_WALKER, DP_ENCH_BINDING_CURSE,
        DP_ENCH_UNBREAKING, DP_ENCH_MENDING, DP_ENCH_VANISHING_CURSE,
    };
    static const uint8_t chestplate[] = {
        DP_ENCH_PROTECTION, DP_ENCH_FIRE_PROTECTION,
        DP_ENCH_BLAST_PROTECTION, DP_ENCH_PROJECTILE_PROTECTION,
        DP_ENCH_THORNS, DP_ENCH_BINDING_CURSE, DP_ENCH_UNBREAKING,
        DP_ENCH_MENDING, DP_ENCH_VANISHING_CURSE,
    };
    static const uint8_t helmet[] = {
        DP_ENCH_PROTECTION, DP_ENCH_FIRE_PROTECTION,
        DP_ENCH_BLAST_PROTECTION, DP_ENCH_PROJECTILE_PROTECTION,
        DP_ENCH_RESPIRATION, DP_ENCH_AQUA_AFFINITY,
        DP_ENCH_BINDING_CURSE, DP_ENCH_UNBREAKING, DP_ENCH_MENDING,
        DP_ENCH_VANISHING_CURSE,
    };
    static const uint8_t leggings[] = {
        DP_ENCH_PROTECTION, DP_ENCH_FIRE_PROTECTION,
        DP_ENCH_BLAST_PROTECTION, DP_ENCH_PROJECTILE_PROTECTION,
        DP_ENCH_BINDING_CURSE, DP_ENCH_UNBREAKING, DP_ENCH_MENDING,
        DP_ENCH_VANISHING_CURSE,
    };
    static const uint8_t crossbow[] = {
        DP_ENCH_UNBREAKING, DP_ENCH_MULTISHOT, DP_ENCH_QUICK_CHARGE,
        DP_ENCH_PIERCING, DP_ENCH_MENDING, DP_ENCH_VANISHING_CURSE,
    };

    *count = 0;
    switch (item)
    {
    case DP_LOOT_GOLDEN_SWORD:
    case DP_LOOT_IRON_SWORD:
    case DP_LOOT_DIAMOND_SWORD:
        *count = sizeof(sword); return sword;
    case DP_LOOT_GOLDEN_AXE:
        *count = sizeof(axe); return axe;
    case DP_LOOT_GOLDEN_HOE:
    case DP_LOOT_GOLDEN_SHOVEL:
    case DP_LOOT_GOLDEN_PICKAXE:
    case DP_LOOT_DIAMOND_PICKAXE:
    case DP_LOOT_DIAMOND_SHOVEL:
        *count = sizeof(digger); return digger;
    case DP_LOOT_GOLDEN_BOOTS:
    case DP_LOOT_LEATHER_BOOTS:
    case DP_LOOT_DIAMOND_BOOTS:
        *count = sizeof(boots); return boots;
    case DP_LOOT_GOLDEN_CHESTPLATE:
    case DP_LOOT_LEATHER_CHESTPLATE:
    case DP_LOOT_DIAMOND_CHESTPLATE:
        *count = sizeof(chestplate); return chestplate;
    case DP_LOOT_GOLDEN_HELMET:
    case DP_LOOT_LEATHER_HELMET:
    case DP_LOOT_DIAMOND_HELMET:
        *count = sizeof(helmet); return helmet;
    case DP_LOOT_GOLDEN_LEGGINGS:
    case DP_LOOT_LEATHER_LEGGINGS:
    case DP_LOOT_DIAMOND_LEGGINGS:
        *count = sizeof(leggings); return leggings;
    case DP_LOOT_CROSSBOW:
        *count = sizeof(crossbow); return crossbow;
    }
    return 0;
}

static int selectRandomEnchantment16(
    uint64_t *rng, int item, int *level)
{
    if (item == DP_LOOT_ENCHANTED_BOOK)
    {
        /*
         * The pre-Soul-Speed ids are deliberately in the same order as the
         * 1.16.1 enchantment registry after the non-discoverable Soul Speed
         * entry is filtered out.
         */
        int enchantment = nextInt(rng, DP_BOOK_ENCHANTMENTS_16);
        int maxLevel = DP_BOOK_MAX_LEVEL_16[enchantment];
        *level = maxLevel > 1 ? 1 + nextInt(rng, maxLevel) : 1;
        return enchantment;
    }
    int count = 0;
    const uint8_t *applicable =
        applicableEnchantments16(item, &count);
    if (count > 0)
    {
        int enchantment = applicable[nextInt(rng, count)];
        int maxLevel = DP_BOOK_MAX_LEVEL_16[enchantment];
        *level = maxLevel > 1 ? 1 + nextInt(rng, maxLevel) : 1;
        return enchantment;
    }
    *level = 0;
    return -1;
}

static void recordEnchantment(StructureLoot *out, int item,
                              int enchantment, int level, int count)
{
    if (!out || item < 0 || item >= DP_LOOT_ITEM_COUNT ||
        enchantment < 0 || enchantment >= DP_ENCH_COUNT ||
        level < 1 || level > DP_ENCH_MAX_LEVEL || count <= 0)
        return;

    for (int i = 0; i < out->enchantmentCount; i++)
    {
        StructureLootEnchantment *entry = out->enchantments + i;
        if (entry->item == item && entry->enchantment == enchantment &&
            entry->level == level)
        {
            entry->count += count;
            return;
        }
    }
    if (out->enchantmentCount >= STRUCTURE_LOOT_MAX_ENCHANTMENTS)
        return;
    StructureLootEnchantment *entry =
        out->enchantments + out->enchantmentCount++;
    entry->item = item;
    entry->enchantment = enchantment;
    entry->level = level;
    entry->count = count;
}

static void consumeStewEffect(uint64_t *rng)
{
    static const uint8_t durationRange[] = {3, 4, 4, 4, 11, 3};
    int effect = nextInt(rng, sizeof(durationRange));
    nextInt(rng, durationRange[effect]);
}

static void generateLootPool(
    StructureLoot *out, uint64_t *rng, int minRolls, int maxRolls,
    const LootEntry16 *entries, int entryCount)
{
    int rolls = lootUniform(rng, minRolls, maxRolls);
    int totalWeight = 0;
    for (int i = 0; i < entryCount; i++)
        totalWeight += entries[i].weight;

    for (int roll = 0; roll < rolls; roll++)
    {
        int selected = 0;
        if (entryCount > 1)
        {
            int value = nextInt(rng, totalWeight);
            for (selected = 0; selected < entryCount - 1; selected++)
            {
                if (value < entries[selected].weight)
                    break;
                value -= entries[selected].weight;
            }
        }
        const LootEntry16 *entry = entries + selected;
        int count = lootUniform(
            rng, entry->minCount, entry->maxCount);
        out->count[entry->item] += count;
        if (entry->function == LOOT_FUNCTION_ENCHANT_RANDOMLY)
        {
            int enchantment, level;
            enchantment = selectRandomEnchantment16(
                rng, entry->item, &level);
            recordEnchantment(
                out, entry->item, enchantment, level, count);
        }
        else if (entry->function == LOOT_FUNCTION_STEW_EFFECT)
            consumeStewEffect(rng);
    }
}

static int getLootTableSeed16(
    uint64_t *lootRng, uint64_t worldSeed, int chunkX, int chunkZ,
    int salt, int beached, int advance)
{
    if (!lootRng ||
        chunkX < -DP_WORLD_BORDER_CHUNKS ||
        chunkX > +DP_WORLD_BORDER_CHUNKS ||
        chunkZ < -DP_WORLD_BORDER_CHUNKS ||
        chunkZ > +DP_WORLD_BORDER_CHUNKS)
        return 0;

    uint64_t populationSeed = getPopulationSeed(
        MC_1_16_1, worldSeed, chunkX * 16, chunkZ * 16);
    uint64_t decoratorRng;
    setSeed(&decoratorRng, populationSeed + salt);
    if (beached)
        nextInt(&decoratorRng, 3);
    skipNextN(&decoratorRng, advance);
    setSeed(lootRng, nextLong(&decoratorRng));
    return 1;
}

static int primaryItem(uint64_t *rng)
{
    static const uint8_t item[] = {
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
    };
    static const uint8_t weight[] = {
        5, 15, 15, 15, 25, 25, 25,
        20, 15, 10, 5, 20, 20, 2,
    };
    int value = nextInt(rng, DP_PRIMARY_WEIGHT);

    for (size_t i = 0; i < sizeof(item) / sizeof(item[0]); i++)
    {
        if (value < weight[i])
            return item[i];
        value -= weight[i];
    }
    return -1; // empty entry, weight 15
}

static int primaryCount(
    uint64_t *rng, int item, int *bookEnchantment, int *bookLevel)
{
    if (bookEnchantment)
        *bookEnchantment = -1;
    if (bookLevel)
        *bookLevel = 0;
    switch (item)
    {
    case DP_LOOT_DIAMOND:       return 1 + nextInt(rng, 3);
    case DP_LOOT_IRON_INGOT:    return 1 + nextInt(rng, 5);
    case DP_LOOT_GOLD_INGOT:    return 2 + nextInt(rng, 6);
    case DP_LOOT_EMERALD:       return 1 + nextInt(rng, 3);
    case DP_LOOT_BONE:          return 4 + nextInt(rng, 3);
    case DP_LOOT_SPIDER_EYE:    return 1 + nextInt(rng, 3);
    case DP_LOOT_ROTTEN_FLESH:  return 3 + nextInt(rng, 5);
    case DP_LOOT_ENCHANTED_BOOK:
        {
            int enchantment = nextInt(rng, DP_BOOK_ENCHANTMENTS_16);
            int maxLevel = DP_BOOK_MAX_LEVEL_16[enchantment];
            int level = 1;
            if (maxLevel > 1)
                level += nextInt(rng, maxLevel);
            if (bookEnchantment)
                *bookEnchantment = enchantment;
            if (bookLevel)
                *bookLevel = level;
            return 1;
        }
    default:
        return 1;
    }
}

int getDesertPyramidLoot16(DesertPyramidLoot *out, uint64_t worldSeed,
                           int chunkX, int chunkZ, int chestIndex)
{
    if (!out || chestIndex < 0 || chestIndex >= 4 ||
        chunkX < -DP_WORLD_BORDER_CHUNKS ||
        chunkX > +DP_WORLD_BORDER_CHUNKS ||
        chunkZ < -DP_WORLD_BORDER_CHUNKS ||
        chunkZ > +DP_WORLD_BORDER_CHUNKS)
        return 0;

    memset(out, 0, sizeof(*out));

    int blockX = chunkX * 16;
    int blockZ = chunkZ * 16;
    uint64_t populationSeed = getPopulationSeed(
        MC_1_16_1, worldSeed, blockX, blockZ);
    uint64_t decoratorRng;
    setSeed(&decoratorRng, populationSeed + DP_DECORATION_SALT_16);
    skipNextN(&decoratorRng, 2 * chestIndex);

    uint64_t lootRng;
    setSeed(&lootRng, nextLong(&decoratorRng));

    int rolls = 2 + nextInt(&lootRng, 3);
    for (int i = 0; i < rolls; i++)
    {
        int item = primaryItem(&lootRng);
        if (item >= 0)
        {
            int enchantment, level;
            out->count[item] += primaryCount(
                &lootRng, item, &enchantment, &level);
            if (enchantment >= 0 && level > 0)
            {
                out->enchantedBook[enchantment][level]++;
                recordEnchantment(
                    out, item, enchantment, level, 1);
            }
        }
    }

    static const uint8_t secondaryItem[] = {
        DP_LOOT_BONE,
        DP_LOOT_GUNPOWDER,
        DP_LOOT_ROTTEN_FLESH,
        DP_LOOT_STRING,
        DP_LOOT_SAND,
    };
    for (int i = 0; i < 4; i++)
    {
        int item = secondaryItem[nextInt(&lootRng, 50) / 10];
        out->count[item] += 1 + nextInt(&lootRng, 8);
    }

    return 1;
}

static void generateBuriedTreasure16(
    StructureLoot *out, uint64_t *rng)
{
    static const LootEntry16 heart[] = {
        {DP_LOOT_HEART_OF_THE_SEA, 1, 1, 1, LOOT_FUNCTION_NONE},
    };
    static const LootEntry16 metals[] = {
        {DP_LOOT_IRON_INGOT, 20, 1, 4, LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLD_INGOT, 10, 1, 4, LOOT_FUNCTION_NONE},
        {DP_LOOT_TNT, 5, 1, 2, LOOT_FUNCTION_NONE},
    };
    static const LootEntry16 valuables[] = {
        {DP_LOOT_EMERALD, 5, 4, 8, LOOT_FUNCTION_NONE},
        {DP_LOOT_DIAMOND, 5, 1, 2, LOOT_FUNCTION_NONE},
        {DP_LOOT_PRISMARINE_CRYSTALS, 5, 1, 5, LOOT_FUNCTION_NONE},
    };
    static const LootEntry16 equipment[] = {
        {DP_LOOT_LEATHER_CHESTPLATE, 1, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_IRON_SWORD, 1, 1, 1, LOOT_FUNCTION_NONE},
    };
    static const LootEntry16 food[] = {
        {DP_LOOT_COOKED_COD, 1, 2, 4, LOOT_FUNCTION_NONE},
        {DP_LOOT_COOKED_SALMON, 1, 2, 4, LOOT_FUNCTION_NONE},
    };
    generateLootPool(out, rng, 1, 1, heart, 1);
    generateLootPool(out, rng, 5, 8, metals, 3);
    generateLootPool(out, rng, 1, 3, valuables, 3);
    generateLootPool(out, rng, 0, 1, equipment, 2);
    generateLootPool(out, rng, 2, 2, food, 2);
}

static void generateRuinedPortal16(
    StructureLoot *out, uint64_t *rng)
{
    static const LootEntry16 entries[] = {
        {DP_LOOT_OBSIDIAN, 40, 1, 2, LOOT_FUNCTION_NONE},
        {DP_LOOT_FLINT, 40, 1, 4, LOOT_FUNCTION_NONE},
        {DP_LOOT_IRON_NUGGET, 40, 9, 18, LOOT_FUNCTION_NONE},
        {DP_LOOT_FLINT_AND_STEEL, 40, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_FIRE_CHARGE, 40, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLDEN_APPLE, 15, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLD_NUGGET, 15, 4, 24, LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLDEN_SWORD, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_AXE, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_HOE, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_SHOVEL, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_PICKAXE, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_BOOTS, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_CHESTPLATE, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_HELMET, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GOLDEN_LEGGINGS, 15, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_GLISTERING_MELON_SLICE, 5, 4, 12,
            LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLDEN_HORSE_ARMOR, 5, 1, 1,
            LOOT_FUNCTION_NONE},
        {DP_LOOT_LIGHT_WEIGHTED_PRESSURE_PLATE, 5, 1, 1,
            LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLDEN_CARROT, 5, 4, 12, LOOT_FUNCTION_NONE},
        {DP_LOOT_CLOCK, 5, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLD_INGOT, 5, 2, 8, LOOT_FUNCTION_NONE},
        {DP_LOOT_BELL, 1, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_ENCHANTED_GOLDEN_APPLE, 1, 1, 1,
            LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLD_BLOCK, 1, 1, 2, LOOT_FUNCTION_NONE},
    };
    generateLootPool(
        out, rng, 4, 8, entries,
        sizeof(entries) / sizeof(entries[0]));
}

static void generateShipwreckSupply16(
    StructureLoot *out, uint64_t *rng)
{
    static const LootEntry16 entries[] = {
        {DP_LOOT_PAPER, 8, 1, 12, LOOT_FUNCTION_NONE},
        {DP_LOOT_POTATO, 7, 2, 6, LOOT_FUNCTION_NONE},
        {DP_LOOT_POISONOUS_POTATO, 7, 2, 6, LOOT_FUNCTION_NONE},
        {DP_LOOT_CARROT, 7, 4, 8, LOOT_FUNCTION_NONE},
        {DP_LOOT_WHEAT, 7, 8, 21, LOOT_FUNCTION_NONE},
        {DP_LOOT_SUSPICIOUS_STEW, 10, 1, 1,
            LOOT_FUNCTION_STEW_EFFECT},
        {DP_LOOT_COAL, 6, 2, 8, LOOT_FUNCTION_NONE},
        {DP_LOOT_ROTTEN_FLESH, 5, 5, 24, LOOT_FUNCTION_NONE},
        {DP_LOOT_PUMPKIN, 2, 1, 3, LOOT_FUNCTION_NONE},
        {DP_LOOT_BAMBOO, 2, 1, 3, LOOT_FUNCTION_NONE},
        {DP_LOOT_GUNPOWDER, 3, 1, 5, LOOT_FUNCTION_NONE},
        {DP_LOOT_TNT, 1, 1, 2, LOOT_FUNCTION_NONE},
        {DP_LOOT_LEATHER_HELMET, 3, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_LEATHER_CHESTPLATE, 3, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_LEATHER_LEGGINGS, 3, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
        {DP_LOOT_LEATHER_BOOTS, 3, 1, 1,
            LOOT_FUNCTION_ENCHANT_RANDOMLY},
    };
    generateLootPool(
        out, rng, 3, 10, entries,
        sizeof(entries) / sizeof(entries[0]));
}

static void generateShipwreckMap16(
    StructureLoot *out, uint64_t *rng)
{
    static const LootEntry16 filledMap[] = {
        {DP_LOOT_FILLED_MAP, 1, 1, 1, LOOT_FUNCTION_NONE},
    };
    static const LootEntry16 entries[] = {
        {DP_LOOT_COMPASS, 1, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_MAP, 1, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_CLOCK, 1, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_PAPER, 20, 1, 10, LOOT_FUNCTION_NONE},
        {DP_LOOT_FEATHER, 10, 1, 5, LOOT_FUNCTION_NONE},
        {DP_LOOT_BOOK, 5, 1, 5, LOOT_FUNCTION_NONE},
    };
    generateLootPool(out, rng, 1, 1, filledMap, 1);
    generateLootPool(out, rng, 3, 3, entries, 6);
}

static void generateShipwreckTreasure16(
    StructureLoot *out, uint64_t *rng)
{
    static const LootEntry16 valuables[] = {
        {DP_LOOT_IRON_INGOT, 90, 1, 5, LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLD_INGOT, 10, 1, 5, LOOT_FUNCTION_NONE},
        {DP_LOOT_EMERALD, 40, 1, 5, LOOT_FUNCTION_NONE},
        {DP_LOOT_DIAMOND, 5, 1, 1, LOOT_FUNCTION_NONE},
        {DP_LOOT_EXPERIENCE_BOTTLE, 5, 1, 1, LOOT_FUNCTION_NONE},
    };
    static const LootEntry16 nuggets[] = {
        {DP_LOOT_IRON_NUGGET, 50, 1, 10, LOOT_FUNCTION_NONE},
        {DP_LOOT_GOLD_NUGGET, 10, 1, 10, LOOT_FUNCTION_NONE},
        {DP_LOOT_LAPIS_LAZULI, 20, 1, 10, LOOT_FUNCTION_NONE},
    };
    generateLootPool(out, rng, 3, 6, valuables, 5);
    generateLootPool(out, rng, 2, 5, nuggets, 3);
}

int getBuriedTreasureLoot16(
    StructureLoot *out, uint64_t worldSeed, int chunkX, int chunkZ)
{
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    uint64_t lootRng;
    if (!getLootTableSeed16(
            &lootRng, worldSeed, chunkX, chunkZ,
            BURIED_DECORATION_SALT_16, 0, 0))
        return 0;
    generateBuriedTreasure16(out, &lootRng);
    return 1;
}

int getRuinedPortalLoot16(
    StructureLoot *out, uint64_t worldSeed, int chunkX, int chunkZ)
{
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    uint64_t lootRng;
    if (!getLootTableSeed16(
            &lootRng, worldSeed, chunkX, chunkZ,
            PORTAL_DECORATION_SALT_16, 0, 0))
        return 0;
    generateRuinedPortal16(out, &lootRng);
    return 1;
}

uint64_t structureLootDecorationSeed26(uint64_t worldSeed, int blockX, int blockZ)
{
    Xoroshiro xr;
    xSetSeed(&xr, worldSeed);
    uint64_t a = xNextLongJ(&xr) | 1;
    uint64_t b = xNextLongJ(&xr) | 1;
    return ((uint64_t)(int64_t)blockX * a +
            (uint64_t)(int64_t)blockZ * b) ^ worldSeed;
}

int getRuinedPortalLoot26(StructureLoot *out, uint64_t worldSeed,
                         int chunkX, int chunkZ, int biomeId, int isNether)
{
    if (!out || biomeId < 0 || chunkX < -DP_WORLD_BORDER_CHUNKS ||
        chunkX > DP_WORLD_BORDER_CHUNKS || chunkZ < -DP_WORLD_BORDER_CHUNKS ||
        chunkZ > DP_WORLD_BORDER_CHUNKS)
        return 0;
    StructureVariant category;
    getVariant(&category, isNether ? Ruined_Portal_N : Ruined_Portal,
               MC_26_2, worldSeed, chunkX * 16, chunkZ * 16, biomeId);
    int cat = isNether ? nether_wastes : category.biome;
    uint64_t rng = chunkGenerateRnd(worldSeed, chunkX, chunkZ);
    int index = 10;
    switch (cat)
    {
    case desert: index = 11; break;
    case jungle: index = 12; (void)nextFloat(&rng); break;
    case mountains: index = 13; /* fall through */
    case plains:
        /* Setup weights are [underground, surface] = [0.5, 0.5].
         * Only the surface setup samples its 0.5 air-pocket probability. */
        if (nextFloat(&rng) >= 0.5f) (void)nextFloat(&rng);
        break;
    case nether_wastes: index = 14; (void)nextFloat(&rng); break;
    case ocean: index = 15; break;
    case swamp: index = 16; break;
    default: return 0;
    }
    int giant = nextFloat(&rng) < 0.05f;
    int portal = giant ? 10 + nextInt(&rng, 3) : nextInt(&rng, 10);
    int rotation = nextInt(&rng, 4);
    int mirror = nextFloat(&rng) >= 0.5f; // FRONT_BACK mirrors local X.
    int sx = PORTAL_SIZES_26_2[portal][0], sz = PORTAL_SIZES_26_2[portal][1];
    int px = sx / 2, pz = sz / 2;
    int minX = 999, minZ = 999, maxX = -999, maxZ = -999;
    for (int i = 0; i < 4; i++)
    {
        int x = (i & 1) ? sx - 1 : 0;
        int z = (i & 2) ? sz - 1 : 0;
        if (mirror) x = -x;
        int tx = x, tz = z;
        if (rotation == 1) { tx = px + pz - z; tz = pz - px + x; }
        if (rotation == 2) { tx = 2 * px - x; tz = 2 * pz - z; }
        if (rotation == 3) { tx = px - pz + z; tz = px + pz - x; }
        if (tx < minX) minX = tx;
        if (tx > maxX) maxX = tx;
        if (tz < minZ) minZ = tz;
        if (tz > maxZ) maxZ = tz;
    }
    /* Portals are placed wholly in their bounding-box CENTER chunk, even
     * when the chest lies across its boundary. */
    int centerX = chunkX * 16 + minX + (maxX - minX + 1) / 2;
    int centerZ = chunkZ * 16 + minZ + (maxZ - minZ + 1) / 2;
    Xoroshiro xr;
    xSetSeed(&xr, structureLootDecorationSeed26(worldSeed,
        floordiv(centerX, 16) * 16, floordiv(centerZ, 16) * 16) + 40000 + index);
    uint64_t lootSeed = xNextLongJ(&xr);
    return lootSeed && generateStructureLootTable16(out, LOOT_TABLE26_RUINED_PORTAL, lootSeed);
}

static uint64_t getCarverSeed16(
    uint64_t worldSeed, int chunkX, int chunkZ)
{
    uint64_t rng;
    setSeed(&rng, worldSeed);
    uint64_t a = nextLong(&rng);
    uint64_t b = nextLong(&rng);
    uint64_t seed =
        (uint64_t)(int64_t)chunkX * a ^
        (uint64_t)(int64_t)chunkZ * b ^
        worldSeed;
    setSeed(&rng, seed);
    return rng;
}

static void getShipwreckChestBlockPos16(
    int *blockX, int *blockZ, int chunkX, int chunkZ,
    int rotation, const ShipTemplate16 *ship, int chest)
{
    const int sizeX = 9;
    int rotationSizeX =
        rotation == 1 || rotation == 3 ? ship->sizeZ : sizeX;
    int rotationSizeZ =
        rotation == 1 || rotation == 3 ? sizeX : ship->sizeZ;
    int sizedX = rotationSizeX - 1;
    int sizedZ = rotationSizeZ - 1;
    int minX, minZ, maxX, maxZ;
    const int pivotX = 4;
    const int pivotZ = 15;

    switch (rotation)
    {
    case 1: /* CLOCKWISE_90 */
        minX = pivotX + pivotZ - sizedX;
        minZ = pivotZ - pivotX;
        maxX = pivotX + pivotZ;
        maxZ = pivotZ - pivotX + sizedZ;
        break;
    case 2: /* CLOCKWISE_180 */
        minX = 2 * pivotX - sizedX;
        minZ = 2 * pivotZ - sizedZ;
        maxX = 2 * pivotX;
        maxZ = 2 * pivotZ;
        break;
    case 3: /* COUNTERCLOCKWISE_90 */
        minX = pivotX - pivotZ;
        minZ = pivotX + pivotZ - sizedZ;
        maxX = pivotX - pivotZ + sizedX;
        maxZ = pivotX + pivotZ;
        break;
    default:
        minX = minZ = 0;
        maxX = sizedX;
        maxZ = sizedZ;
        break;
    }
    minX += chunkX * 16;
    maxX += chunkX * 16;
    minZ += chunkZ * 16;
    maxZ += chunkZ * 16;

    int pieceX, pieceZ;
    switch (rotation)
    {
    case 1: pieceX = maxX; pieceZ = minZ; break;
    case 2: pieceX = maxX; pieceZ = maxZ; break;
    case 3: pieceX = minX; pieceZ = maxZ; break;
    default: pieceX = minX; pieceZ = minZ; break;
    }

    switch (rotation)
    {
    case 1:
        *blockX = pieceX - ship->z[chest];
        *blockZ = pieceZ + ship->x[chest];
        break;
    case 2:
        *blockX = pieceX - ship->x[chest];
        *blockZ = pieceZ - ship->z[chest];
        break;
    case 3:
        *blockX = pieceX + ship->z[chest];
        *blockZ = pieceZ - ship->x[chest];
        break;
    default:
        *blockX = pieceX + ship->x[chest];
        *blockZ = pieceZ + ship->z[chest];
        break;
    }
}

int getShipwreckLoot16(
    StructureLoot out[SHIPWRECK_CHEST_COUNT],
    uint8_t present[SHIPWRECK_CHEST_COUNT],
    uint64_t worldSeed, int chunkX, int chunkZ, int isBeached)
{
    if (!out || !present ||
        chunkX < -DP_WORLD_BORDER_CHUNKS ||
        chunkX > +DP_WORLD_BORDER_CHUNKS ||
        chunkZ < -DP_WORLD_BORDER_CHUNKS ||
        chunkZ > +DP_WORLD_BORDER_CHUNKS)
        return 0;
    memset(out, 0, sizeof(*out) * SHIPWRECK_CHEST_COUNT);
    memset(present, 0, SHIPWRECK_CHEST_COUNT);

    uint64_t carverRng = getCarverSeed16(
        worldSeed, chunkX, chunkZ);
    int rotation = nextInt(&carverRng, 4);
    const uint8_t *types = isBeached
        ? SHIP_BEACHED_TYPES_16 : SHIP_OCEAN_TYPES_16;
    int typeCount = isBeached
        ? sizeof(SHIP_BEACHED_TYPES_16)
        : sizeof(SHIP_OCEAN_TYPES_16);
    const ShipTemplate16 *ship =
        SHIP_TEMPLATES_16 + types[nextInt(&carverRng, typeCount)];

    int chestChunkX[SHIPWRECK_CHEST_COUNT];
    int chestChunkZ[SHIPWRECK_CHEST_COUNT];
    for (int i = 0; i < ship->count; i++)
    {
        int blockX, blockZ;
        getShipwreckChestBlockPos16(
            &blockX, &blockZ, chunkX, chunkZ,
            rotation, ship, i);
        chestChunkX[i] = floordiv(blockX, 16);
        chestChunkZ[i] = floordiv(blockZ, 16);
    }

    for (int i = 0; i < ship->count; i++)
    {
        int numberInChunk = 0;
        int indexInChunk = 0;
        for (int j = 0; j < ship->count; j++)
        {
            if (chestChunkX[j] == chestChunkX[i] &&
                chestChunkZ[j] == chestChunkZ[i])
            {
                numberInChunk++;
                if (j < i)
                    indexInChunk++;
            }
        }
        uint64_t lootRng;
        if (!getLootTableSeed16(
                &lootRng, worldSeed,
                chestChunkX[i], chestChunkZ[i],
                SHIPWRECK_DECORATION_SALT_16,
                !!isBeached,
                2 * numberInChunk + 2 * indexInChunk))
            return 0;

        int kind = ship->kind[i];
        present[kind] = 1;
        if (kind == SHIPWRECK_CHEST_SUPPLY)
            generateShipwreckSupply16(out + kind, &lootRng);
        else if (kind == SHIPWRECK_CHEST_MAP)
            generateShipwreckMap16(out + kind, &lootRng);
        else
            generateShipwreckTreasure16(out + kind, &lootRng);
    }
    return 1;
}

int generateStructureLootTable16(
    StructureLoot *out, int table, uint64_t lootTableSeed)
{
    const LootTableDefinition16 *definition;
    const LootTablePool16 *tablePools;
    const LootTableEntry16 *tableEntries;
    if (!out || !resolveStructureLootTable16(
            table, &definition, &tablePools, &tableEntries, 0))
        return 0;

    memset(out, 0, sizeof(*out));
    uint64_t rng;
    setSeed(&rng, lootTableSeed);

    for (int poolIndex = 0;
         poolIndex < definition->poolCount; poolIndex++)
    {
        const LootTablePool16 *pool =
            tablePools +
            definition->firstPool + poolIndex;
        int rolls = lootUniform(
            &rng, pool->minRolls, pool->maxRolls);
        int totalWeight = 0;
        for (int entryIndex = 0;
             entryIndex < pool->entryCount; entryIndex++)
        {
            totalWeight += tableEntries[
                pool->firstEntry + entryIndex].weight;
        }

        for (int roll = 0; roll < rolls; roll++)
        {
            int selected = 0;
            if (pool->entryCount > 1)
            {
                int value = nextInt(&rng, totalWeight);
                for (selected = 0;
                     selected < pool->entryCount - 1; selected++)
                {
                    const LootTableEntry16 *candidate =
                        tableEntries +
                        pool->firstEntry + selected;
                    if (value < candidate->weight)
                        break;
                    value -= candidate->weight;
                }
            }
            const LootTableEntry16 *entry =
                tableEntries +
                pool->firstEntry + selected;
            if (entry->flags & LT16_EMPTY)
                continue;

            /*
             * All 1.16.1 tables generated by the pinned extractor use either
             * set_damage -> enchant_randomly or a (possibly constant)
             * set_count -> enchant_randomly sequence.
             */
            if (entry->flags & LT16_DAMAGE)
                (void) nextFloat(&rng);
            int count = lootUniform(
                &rng, entry->minCount, entry->maxCount);

            int enchantment = -1;
            int level = 0;
            if (entry->flags & LT16_ENCHANT)
            {
                if (entry->flags & LT16_SOUL_SPEED)
                {
                    /*
                     * Vanilla still calls nextInt(1) when a one-element
                     * explicit enchantment list is supplied.
                     */
                    (void) nextInt(&rng, 1);
                    enchantment = DP_ENCH_SOUL_SPEED;
                    level = 1 + nextInt(&rng, 3);
                }
                else if (table >= LOOT_TABLE26_BASTION_BRIDGE)
                {
                    enchantment = selectModernEnchantment(&rng, entry->item, &level);
                }
                else
                {
                    enchantment = selectRandomEnchantment16(
                        &rng, entry->item, &level);
                }
            }

            out->count[entry->item] += count;
            if (enchantment >= 0 && level > 0)
            {
                recordEnchantment(
                    out, entry->item, enchantment, level, count);
                if (entry->item == DP_LOOT_ENCHANTED_BOOK)
                    out->enchantedBook[enchantment][level] += count;
            }
        }
    }
    return 1;
}

const char *structureLootTable16Name(int table)
{
    const char *name = 0;
    if (!resolveStructureLootTable16(
            table, 0, 0, 0, &name))
        return 0;
    return name;
}

const char *desertPyramidLootItemName(int item)
{
    return structureLootItemName(item);
}

const char *structureLootItemName(int item)
{
    static const char *name[DP_LOOT_ITEM_COUNT] = {
        "diamond",
        "iron_ingot",
        "gold_ingot",
        "emerald",
        "bone",
        "spider_eye",
        "rotten_flesh",
        "saddle",
        "iron_horse_armor",
        "golden_horse_armor",
        "diamond_horse_armor",
        "enchanted_book",
        "golden_apple",
        "enchanted_golden_apple",
        "gunpowder",
        "string",
        "sand",
        "heart_of_the_sea",
        "tnt",
        "prismarine_crystals",
        "leather_chestplate",
        "iron_sword",
        "cooked_cod",
        "cooked_salmon",
        "obsidian",
        "flint",
        "iron_nugget",
        "flint_and_steel",
        "fire_charge",
        "gold_nugget",
        "golden_sword",
        "golden_axe",
        "golden_hoe",
        "golden_shovel",
        "golden_pickaxe",
        "golden_boots",
        "golden_chestplate",
        "golden_helmet",
        "golden_leggings",
        "glistering_melon_slice",
        "light_weighted_pressure_plate",
        "golden_carrot",
        "clock",
        "gold_block",
        "bell",
        "filled_map",
        "compass",
        "map",
        "paper",
        "feather",
        "book",
        "potato",
        "poisonous_potato",
        "carrot",
        "wheat",
        "suspicious_stew",
        "coal",
        "pumpkin",
        "bamboo",
        "leather_helmet",
        "leather_leggings",
        "leather_boots",
        "experience_bottle",
        "lapis_lazuli",
        "bread",
        "iron_helmet",
        "porkchop",
        "beef",
        "mutton",
        "stick",
        "clay_ball",
        "green_dye",
        "cactus",
        "cod",
        "salmon",
        "water_bucket",
        "barrel",
        "wheat_seeds",
        "arrow",
        "egg",
        "flower_pot",
        "stone",
        "stone_bricks",
        "yellow_dye",
        "smooth_stone",
        "dandelion",
        "poppy",
        "apple",
        "oak_sapling",
        "grass",
        "tall_grass",
        "acacia_sapling",
        "torch",
        "bucket",
        "white_wool",
        "black_wool",
        "gray_wool",
        "brown_wool",
        "light_gray_wool",
        "shears",
        "blue_ice",
        "snow_block",
        "beetroot_seeds",
        "beetroot_soup",
        "furnace",
        "snowball",
        "fern",
        "large_fern",
        "sweet_berries",
        "pumpkin_seeds",
        "pumpkin_pie",
        "spruce_sapling",
        "spruce_sign",
        "spruce_log",
        "leather",
        "redstone",
        "iron_pickaxe",
        "iron_shovel",
        "iron_chestplate",
        "iron_leggings",
        "iron_boots",
        "lodestone",
        "crossbow",
        "spectral_arrow",
        "gilded_blackstone",
        "crying_obsidian",
        "diamond_shovel",
        "netherite_scrap",
        "ancient_debris",
        "glowstone",
        "soul_sand",
        "crimson_nylium",
        "cooked_porkchop",
        "crimson_fungus",
        "crimson_roots",
        "piglin_banner_pattern",
        "music_disc_pigstep",
        "chain",
        "magma_cream",
        "bone_block",
        "netherite_ingot",
        "diamond_sword",
        "diamond_chestplate",
        "diamond_helmet",
        "diamond_leggings",
        "diamond_boots",
        "quartz",
        "dead_bush",
        "any_container",
        "diamond_pickaxe",
        "iron_block",
        "snout_armor_trim_smithing_template",
        "netherite_upgrade_smithing_template",
        "iron_chain",
        "diamond_spear",
    };
    if (item < 0 || item >= DP_LOOT_ITEM_COUNT)
        return 0;
    return name[item];
}

static int lootTableRangeHasItem16(
    int firstTable, int endTable, int item)
{
    for (int table = firstTable; table < endTable; table++)
    {
        const LootTableDefinition16 *definition;
        const LootTablePool16 *tablePools;
        const LootTableEntry16 *tableEntries;
        if (!resolveStructureLootTable16(
                table, &definition, &tablePools,
                &tableEntries, 0))
            return 0;
        for (int poolIndex = 0;
             poolIndex < definition->poolCount; poolIndex++)
        {
            const LootTablePool16 *pool =
                tablePools +
                definition->firstPool + poolIndex;
            for (int entryIndex = 0;
                 entryIndex < pool->entryCount; entryIndex++)
            {
                if (tableEntries[
                        pool->firstEntry + entryIndex].item == item)
                    return 1;
            }
        }
    }
    return 0;
}

static int bastionLootTableRange16(
    int profile, int *firstTable, int *endTable)
{
    if (profile == BASTION_LOOT_PROFILE_1_16_1)
    {
        *firstTable = LOOT_TABLE16_BASTION_BRIDGE;
        *endTable = LOOT_TABLE16_BASTION_TREASURE + 1;
        return 1;
    }
    if (profile == BASTION_LOOT_PROFILE_1_16_2_TO_1_16_5)
    {
        *firstTable = LOOT_TABLE16_BASTION_BRIDGE_1_16_5;
        *endTable = LOOT_TABLE16_BASTION_TREASURE_1_16_5 + 1;
        return 1;
    }
    if (profile == LOOT_PROFILE_26_2)
    {
        *firstTable = LOOT_TABLE26_BASTION_BRIDGE;
        *endTable = LOOT_TABLE26_BASTION_TREASURE + 1;
        return 1;
    }
    return 0;
}

int structureLootItemAvailable(int structureType, int item,
                               int bastionLootProfile)
{
    if (item < 0 || item >= DP_LOOT_ITEM_COUNT)
        return 0;
    if (item == DP_LOOT_ANY_CONTAINER)
        return 1;
    switch (structureType)
    {
    case Village:
        return lootTableRangeHasItem16(
            LOOT_TABLE16_VILLAGE_ARMORER,
            LOOT_TABLE16_BASTION_BRIDGE, item);
    case Bastion:
    {
        int firstTable, endTable;
        return bastionLootTableRange16(
                   bastionLootProfile, &firstTable, &endTable) &&
            lootTableRangeHasItem16(firstTable, endTable, item);
    }
    case Desert_Pyramid:
        return item <= DP_LOOT_SAND;
    case Treasure:
        switch (item)
        {
        case DP_LOOT_HEART_OF_THE_SEA:
        case DP_LOOT_IRON_INGOT:
        case DP_LOOT_GOLD_INGOT:
        case DP_LOOT_TNT:
        case DP_LOOT_EMERALD:
        case DP_LOOT_DIAMOND:
        case DP_LOOT_PRISMARINE_CRYSTALS:
        case DP_LOOT_LEATHER_CHESTPLATE:
        case DP_LOOT_IRON_SWORD:
        case DP_LOOT_COOKED_COD:
        case DP_LOOT_COOKED_SALMON:
            return 1;
        }
        return 0;
    case Ruined_Portal:
    case Ruined_Portal_N:
        if (bastionLootProfile == LOOT_PROFILE_26_2)
            return lootTableRangeHasItem16(LOOT_TABLE26_RUINED_PORTAL,
                LOOT_TABLE26_RUINED_PORTAL + 1, item);
        switch (item)
        {
        case DP_LOOT_OBSIDIAN:
        case DP_LOOT_FLINT:
        case DP_LOOT_IRON_NUGGET:
        case DP_LOOT_FLINT_AND_STEEL:
        case DP_LOOT_FIRE_CHARGE:
        case DP_LOOT_GOLDEN_APPLE:
        case DP_LOOT_GOLD_NUGGET:
        case DP_LOOT_GOLDEN_SWORD:
        case DP_LOOT_GOLDEN_AXE:
        case DP_LOOT_GOLDEN_HOE:
        case DP_LOOT_GOLDEN_SHOVEL:
        case DP_LOOT_GOLDEN_PICKAXE:
        case DP_LOOT_GOLDEN_BOOTS:
        case DP_LOOT_GOLDEN_CHESTPLATE:
        case DP_LOOT_GOLDEN_HELMET:
        case DP_LOOT_GOLDEN_LEGGINGS:
        case DP_LOOT_GLISTERING_MELON_SLICE:
        case DP_LOOT_GOLDEN_HORSE_ARMOR:
        case DP_LOOT_LIGHT_WEIGHTED_PRESSURE_PLATE:
        case DP_LOOT_GOLDEN_CARROT:
        case DP_LOOT_CLOCK:
        case DP_LOOT_GOLD_INGOT:
        case DP_LOOT_BELL:
        case DP_LOOT_ENCHANTED_GOLDEN_APPLE:
        case DP_LOOT_GOLD_BLOCK:
            return 1;
        }
        return 0;
    case Shipwreck:
        switch (item)
        {
        case DP_LOOT_FILLED_MAP:
        case DP_LOOT_COMPASS:
        case DP_LOOT_MAP:
        case DP_LOOT_CLOCK:
        case DP_LOOT_PAPER:
        case DP_LOOT_FEATHER:
        case DP_LOOT_BOOK:
        case DP_LOOT_POTATO:
        case DP_LOOT_POISONOUS_POTATO:
        case DP_LOOT_CARROT:
        case DP_LOOT_WHEAT:
        case DP_LOOT_SUSPICIOUS_STEW:
        case DP_LOOT_COAL:
        case DP_LOOT_ROTTEN_FLESH:
        case DP_LOOT_PUMPKIN:
        case DP_LOOT_BAMBOO:
        case DP_LOOT_GUNPOWDER:
        case DP_LOOT_TNT:
        case DP_LOOT_LEATHER_HELMET:
        case DP_LOOT_LEATHER_CHESTPLATE:
        case DP_LOOT_LEATHER_LEGGINGS:
        case DP_LOOT_LEATHER_BOOTS:
        case DP_LOOT_IRON_INGOT:
        case DP_LOOT_GOLD_INGOT:
        case DP_LOOT_EMERALD:
        case DP_LOOT_DIAMOND:
        case DP_LOOT_EXPERIENCE_BOTTLE:
        case DP_LOOT_IRON_NUGGET:
        case DP_LOOT_GOLD_NUGGET:
        case DP_LOOT_LAPIS_LAZULI:
            return 1;
        }
        return 0;
    }
    return 0;
}

static int enchantmentAppliesToItem16(int item, int enchantment)
{
    if (item == DP_LOOT_ENCHANTED_BOOK)
        return enchantment >= 0 &&
            enchantment < DP_BOOK_ENCHANTMENTS_16;
    int count = 0;
    const uint8_t *applicable =
        applicableEnchantments16(item, &count);
    for (int i = 0; i < count; i++)
        if (applicable[i] == enchantment)
            return 1;
    return 0;
}

int structureLootEnchantmentAvailable(int structureType, int item,
                                      int enchantment,
                                      int bastionLootProfile)
{
    if ((structureType == Ruined_Portal || structureType == Ruined_Portal_N) &&
        bastionLootProfile == LOOT_PROFILE_26_2)
        return structureLootItemAvailable(structureType, item, bastionLootProfile) &&
            modernEnchantmentLevel(item, enchantment) > 0;
    if (structureType != Bastion || item < 0 ||
        item >= DP_LOOT_ITEM_COUNT || enchantment < 0 ||
        enchantment >= DP_ENCH_COUNT)
        return 0;

    int hasRandomEnchant = 0;
    int hasSoulSpeed = 0;
    int firstTable, endTable;
    if (!bastionLootTableRange16(
            bastionLootProfile, &firstTable, &endTable))
        return 0;
    for (int table = firstTable; table < endTable; table++)
    {
        const LootTableDefinition16 *definition;
        const LootTablePool16 *tablePools;
        const LootTableEntry16 *tableEntries;
        if (!resolveStructureLootTable16(
                table, &definition, &tablePools,
                &tableEntries, 0))
            return 0;
        for (int poolIndex = 0;
             poolIndex < definition->poolCount; poolIndex++)
        {
            const LootTablePool16 *pool =
                tablePools +
                definition->firstPool + poolIndex;
            for (int entryIndex = 0;
                 entryIndex < pool->entryCount; entryIndex++)
            {
                const LootTableEntry16 *entry =
                    tableEntries +
                    pool->firstEntry + entryIndex;
                if (entry->item != item ||
                    !(entry->flags & LT16_ENCHANT))
                    continue;
                if (entry->flags & LT16_SOUL_SPEED)
                    hasSoulSpeed = 1;
                else
                    hasRandomEnchant = 1;
            }
        }
    }
    if (enchantment == DP_ENCH_SOUL_SPEED)
        return hasSoulSpeed;
    if (bastionLootProfile == LOOT_PROFILE_26_2)
        return hasRandomEnchant && modernEnchantmentLevel(item, enchantment) > 0;
    return hasRandomEnchant &&
        enchantmentAppliesToItem16(item, enchantment);
}

int structureLootItemCanBeEnchanted(int structureType, int item,
                                    int bastionLootProfile)
{
    for (int enchantment = 0;
         enchantment < DP_ENCH_COUNT; enchantment++)
    {
        if (structureLootEnchantmentAvailable(
                structureType, item, enchantment,
                bastionLootProfile))
            return 1;
    }
    return 0;
}

const char *desertPyramidEnchantmentName(int enchantment)
{
    static const char *name[DP_ENCH_COUNT] = {
        "protection",
        "fire_protection",
        "feather_falling",
        "blast_protection",
        "projectile_protection",
        "respiration",
        "aqua_affinity",
        "thorns",
        "depth_strider",
        "frost_walker",
        "binding_curse",
        "sharpness",
        "smite",
        "bane_of_arthropods",
        "knockback",
        "fire_aspect",
        "looting",
        "sweeping",
        "efficiency",
        "silk_touch",
        "unbreaking",
        "fortune",
        "power",
        "punch",
        "flame",
        "infinity",
        "luck_of_the_sea",
        "lure",
        "loyalty",
        "impaling",
        "riptide",
        "channeling",
        "multishot",
        "quick_charge",
        "piercing",
        "mending",
        "vanishing_curse",
        "soul_speed",
        "lunge",
    };
    if (enchantment < 0 || enchantment >= DP_ENCH_COUNT)
        return 0;
    return name[enchantment];
}

int desertPyramidEnchantmentMaxLevel(int enchantment)
{
    if (enchantment < 0 || enchantment >= DP_ENCH_COUNT)
        return 0;
    if (enchantment == DP_ENCH_SOUL_SPEED || enchantment == DP_ENCH_LUNGE)
        return 3;
    return DP_BOOK_MAX_LEVEL_16[enchantment];
}
