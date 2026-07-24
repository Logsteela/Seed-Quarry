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
    DP_PRIMARY_WEIGHT = 232,
    DP_BOOK_ENCHANTMENTS_16 = DP_ENCH_COUNT,
    DP_WORLD_BORDER_CHUNKS = 1875000,
};

static const uint8_t DP_BOOK_MAX_LEVEL_16[DP_BOOK_ENCHANTMENTS_16] = {
    4, 4, 4, 4, 4, 3, 1, 3, 3, 2, 1,
    5, 5, 5, 2, 2, 3, 3, 5, 1, 3, 3,
    5, 2, 1, 1, 3, 3, 3, 5, 3, 1, 1,
    3, 4, 1, 1,
};

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
                out->enchantedBook[enchantment][level]++;
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

const char *desertPyramidLootItemName(int item)
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
    };
    if (item < 0 || item >= DP_LOOT_ITEM_COUNT)
        return 0;
    return name[item];
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
    };
    if (enchantment < 0 || enchantment >= DP_ENCH_COUNT)
        return 0;
    return name[enchantment];
}

int desertPyramidEnchantmentMaxLevel(int enchantment)
{
    if (enchantment < 0 || enchantment >= DP_ENCH_COUNT)
        return 0;
    return DP_BOOK_MAX_LEVEL_16[enchantment];
}
