#include "biomenoise.h"
#include "biomes.h"
#include "finders.h"
#include "loot.h"
#include "util.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void assertStructureConfig(int stype, int mc, int spacing,
                                  int separation, int salt)
{
    StructureConfig config;
    assert(getStructureConfig(stype, mc, &config));
    assert(config.regionSize == spacing);
    assert(config.chunkRange == spacing - separation);
    assert(config.salt == salt);
}

static void assertStructurePos(int stype, int mc, uint64_t seed,
                               int regX, int regZ, int x, int z)
{
    Pos pos;
    assert(getStructurePos(stype, mc, seed, regX, regZ, &pos));
    assert(pos.x == x && pos.z == z);
}

static void assertVillageBastionLootTables(void)
{
    StructureLoot loot;

    assert(generateStructureLootTable16(
        &loot, LOOT_TABLE16_VILLAGE_WEAPONSMITH, 123456789ULL));
    assert(loot.count[DP_LOOT_DIAMOND] == 3);
    assert(loot.count[DP_LOOT_IRON_PICKAXE] == 1);
    assert(loot.count[DP_LOOT_OBSIDIAN] == 6);
    assert(loot.count[DP_LOOT_SADDLE] == 1);

    assert(generateStructureLootTable16(
        &loot, LOOT_TABLE16_BASTION_BRIDGE, 123456789ULL));
    assert(loot.count[DP_LOOT_ARROW] == 28);
    assert(loot.count[DP_LOOT_CROSSBOW] == 1);
    assert(loot.count[DP_LOOT_IRON_INGOT] == 2);
    assert(loot.count[DP_LOOT_LODESTONE] == 1);

    assert(generateStructureLootTable16(
        &loot, LOOT_TABLE16_BASTION_OTHER, 123456789ULL));
    assert(loot.count[DP_LOOT_ARROW] == 12);
    assert(loot.count[DP_LOOT_GOLD_NUGGET] == 3);
    assert(loot.count[DP_LOOT_GOLDEN_BOOTS] == 1);
    assert(loot.count[DP_LOOT_GOLDEN_CHESTPLATE] == 1);
    assert(loot.count[DP_LOOT_IRON_NUGGET] == 3);
    assert(loot.count[DP_LOOT_MAGMA_CREAM] == 4);

    assert(generateStructureLootTable16(
        &loot, LOOT_TABLE16_BASTION_TREASURE, 123456789ULL));
    assert(loot.count[DP_LOOT_CRYING_OBSIDIAN] == 7);
    assert(loot.count[DP_LOOT_DIAMOND_HELMET] == 1);
    assert(loot.count[DP_LOOT_NETHERITE_INGOT] == 1);

    /* One-element Soul Speed lists still consume nextInt(1). */
    assert(generateStructureLootTable16(
        &loot, LOOT_TABLE16_BASTION_OTHER, 15ULL));
    assert(loot.count[DP_LOOT_ENCHANTED_BOOK] == 1);
    assert(loot.enchantedBook[DP_ENCH_SOUL_SPEED][2] == 1);
}

static int templateOrientation(const StructureVariant *sv)
{
    if (sv->mirror)
        return sv->rotation ? 3 : 2;
    return sv->rotation;
}

static void assertStructureVariants(void)
{
    unsigned portalUnderground = 0;
    unsigned portalAirpocket = 0;
    unsigned portalGiant = 0;
    unsigned portalMirror = 0;
    unsigned portalRotation = 0;
    unsigned portalNormalStarts = 0;
    unsigned portalGiantStarts = 0;
    unsigned villageRotation = 0;
    unsigned bastionStarts = 0;
    unsigned bastionRotation = 0;
    unsigned iglooBasement = 0;
    unsigned iglooOrientation = 0;
    unsigned iglooSizes = 0;
    unsigned ancientStarts = 0;
    unsigned ancientRotation = 0;
    unsigned chamberStarts = 0;
    unsigned chamberRotation = 0;
    unsigned templeOrientation = 0;

    // Consecutive Java Random seeds have correlated first outputs. The odd
    // golden-ratio step spreads this sample over all low 48-bit seed states.
    for (uint64_t i = 0; i < 4096; i++)
    {
        uint64_t seed = i * 0x9e3779b97f4a7c15ULL;
        StructureVariant sv, sv161, sv165;

        assert(getVariant(&sv, Ruined_Portal, MC_1_16_1,
                          seed, 0, 0, plains));
        assert(sv.biome == plains);
        assert(sv.start >= 1 && sv.start <= (sv.giant ? 3 : 10));
        assert(sv.rotation < 4 && sv.mirror < 2);
        if (sv.underground)
            assert(sv.airpocket);
        portalUnderground |= 1U << sv.underground;
        portalAirpocket |= 1U << sv.airpocket;
        portalGiant |= 1U << sv.giant;
        portalMirror |= 1U << sv.mirror;
        portalRotation |= 1U << sv.rotation;
        if (sv.giant)
            portalGiantStarts |= 1U << (sv.start - 1);
        else
            portalNormalStarts |= 1U << (sv.start - 1);

        assert(getVariant(&sv, Ruined_Portal, MC_1_16_1,
                          seed, 0, 0, desert));
        assert(sv.biome == desert);
        assert(!sv.underground && !sv.airpocket);

        assert(getVariant(&sv, Ruined_Portal_N, MC_1_16_1,
                          seed, 0, 0, nether_wastes));
        assert(sv.biome == nether_wastes);
        assert(!sv.underground && !sv.airpocket);

        assert(getVariant(&sv, Village, MC_1_16_1,
                          seed, 0, 0, plains));
        assert(sv.rotation < 4 && sv.start < 4);
        villageRotation |= 1U << sv.rotation;

        assert(getVariant(&sv161, Bastion, MC_1_16_1,
                          seed, 0, 0, -1));
        assert(getVariant(&sv165, Bastion, MC_1_16_5,
                          seed, 0, 0, -1));
        assert(sv161.start < 4 && sv161.rotation < 4);
        assert(sv161.start == sv165.rotation);
        assert(sv161.rotation == sv165.start);
        bastionStarts |= 1U << sv161.start;
        bastionRotation |= 1U << sv161.rotation;

        assert(getVariant(&sv, Igloo, MC_1_16_1,
                          seed, 0, 0, snowy_tundra));
        assert(sv.basement < 2);
        assert(sv.size >= 4 && sv.size <= 11);
        assert(templateOrientation(&sv) < 4);
        iglooBasement |= 1U << sv.basement;
        iglooOrientation |= 1U << templateOrientation(&sv);
        iglooSizes |= 1U << (sv.size - 4);

        assert(getVariant(&sv, Ancient_City, MC_1_19,
                          seed, 0, 0, -1));
        assert(sv.start >= 1 && sv.start <= 3 && sv.rotation < 4);
        ancientStarts |= 1U << (sv.start - 1);
        ancientRotation |= 1U << sv.rotation;

        assert(getVariant(&sv, Trial_Chambers, MC_1_21_1,
                          seed, 0, 0, -1));
        assert(sv.start < 2 && sv.rotation < 4);
        chamberStarts |= 1U << sv.start;
        chamberRotation |= 1U << sv.rotation;

        assert(getVariant(&sv, Desert_Pyramid, MC_1_20,
                          seed, 0, 0, desert));
        assert(templateOrientation(&sv) < 4);
        templeOrientation |= 1U << templateOrientation(&sv);
    }

    assert(portalUnderground == 0x3);
    assert(portalAirpocket == 0x3);
    assert(portalGiant == 0x3);
    assert(portalMirror == 0x3);
    assert(portalRotation == 0xf);
    assert(portalNormalStarts == 0x3ff);
    assert(portalGiantStarts == 0x7);
    assert(villageRotation == 0xf);
    assert(bastionStarts == 0xf);
    assert(bastionRotation == 0xf);
    assert(iglooBasement == 0x3);
    assert(iglooOrientation == 0xf);
    assert(iglooSizes == 0xff);
    assert(ancientStarts == 0x7);
    assert(ancientRotation == 0xf);
    assert(chamberStarts == 0x3);
    assert(chamberRotation == 0xf);
    assert(templeOrientation == 0xf);
}

static void assertDesertPyramidLoot(void)
{
    // Generated by MineMap 1.0.26 / mc_feature_java 1.171.1 for Java
    // 1.16.1/1.16.5. Each group has Vanilla chest RNG indexes 0 through 3.
    struct LootCase
    {
        uint64_t seed;
        int chunkX, chunkZ;
        uint16_t expected[4][DP_LOOT_ITEM_COUNT];
    };
    static const struct LootCase cases[] = {
        {
            3515201313347228787ULL, 17, -9, {
                {0, 0, 0, 0,  0, 3, 9, 0, 0, 0, 0, 0, 0, 0, 8, 0, 1},
                {0, 0, 0, 1,  5, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 9},
                {0, 0, 3, 0, 15, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4},
                {1, 0, 0, 2, 20, 0, 6, 0, 0, 0, 0, 1, 0, 0, 4, 0, 0},
            }
        },
        {
            123ULL, 173, -73, {
                {0, 0, 0, 0, 9, 2,  7, 0, 0, 0, 0, 0, 0, 0,  5, 6, 4},
                {0, 0, 0, 0, 5, 2,  0, 1, 0, 0, 0, 0, 0, 0,  6, 0, 6},
                {0, 4, 0, 0, 0, 0, 13, 0, 1, 0, 0, 0, 0, 0,  7, 8, 0},
                {0, 0, 0, 0, 3, 0,  5, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0},
            }
        },
        {
            3119024338951782547ULL, 47, -47, {
                {0, 0, 7, 0, 1, 3,  1, 1, 0, 0, 0, 0, 0, 0, 0, 11,  0},
                {0, 2, 0, 1, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5,  0,  2},
                {0, 0, 0, 0, 0, 2, 14, 0, 1, 0, 0, 2, 0, 0, 7,  0,  0},
                {0, 5, 0, 6, 6, 0,  0, 0, 0, 0, 0, 0, 0, 0, 3,  0, 12},
            }
        },
        {
            UINT64_MAX, -1875000, 1875000, {
                {0, 2, 0, 0, 4, 0,  0, 0, 0, 0, 0, 1, 2, 0, 14, 6, 0},
                {0, 0, 0, 3, 4, 0,  4, 0, 1, 0, 0, 0, 0, 0,  0, 2, 9},
                {0, 0, 0, 0, 8, 0, 15, 1, 0, 0, 0, 0, 0, 0,  7, 0, 0},
                {0, 4, 0, 0, 0, 3,  7, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0},
            }
        },
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
    {
        for (int chest = 0; chest < 4; chest++)
        {
            DesertPyramidLoot loot;
            assert(getDesertPyramidLoot16(
                &loot, cases[c].seed, cases[c].chunkX, cases[c].chunkZ, chest));
            assert(memcmp(
                loot.count, cases[c].expected[chest], sizeof(loot.count)) == 0);
            int bookCount = 0;
            for (int enchantment = 0; enchantment < DP_ENCH_COUNT; enchantment++)
                for (int level = 1; level <= DP_ENCH_MAX_LEVEL; level++)
                    bookCount += loot.enchantedBook[enchantment][level];
            assert(bookCount == loot.count[DP_LOOT_ENCHANTED_BOOK]);
            if (c == 0 && chest == 3)
                assert(loot.enchantedBook[DP_ENCH_SILK_TOUCH][1] == 1);
        }
    }

    DesertPyramidLoot loot;
    assert(!getDesertPyramidLoot16(&loot, 0, 17, -9, -1));
    assert(!getDesertPyramidLoot16(&loot, 0, 17, -9, 4));
    assert(!getDesertPyramidLoot16(&loot, 0, 1875001, 0, 0));
    assert(!getDesertPyramidLoot16(&loot, 0, 0, -1875001, 0));
    assert(!getDesertPyramidLoot16(NULL, 0, 17, -9, 0));
    assert(strcmp(desertPyramidLootItemName(DP_LOOT_DIAMOND), "diamond") == 0);
    assert(desertPyramidLootItemName(-1) == NULL);
    assert(desertPyramidLootItemName(DP_LOOT_ITEM_COUNT) == NULL);
    assert(strcmp(
        desertPyramidEnchantmentName(DP_ENCH_SILK_TOUCH),
        "silk_touch") == 0);
    assert(desertPyramidEnchantmentMaxLevel(DP_ENCH_SHARPNESS) == 5);
    assert(desertPyramidEnchantmentMaxLevel(DP_ENCH_MENDING) == 1);
    assert(desertPyramidEnchantmentName(DP_ENCH_COUNT) == NULL);
}

static void assertAdditionalStructureLoot(void)
{
    StructureLoot loot;
    assert(getBuriedTreasureLoot16(&loot, 123ULL, 56, -105));
    assert(loot.count[DP_LOOT_HEART_OF_THE_SEA] == 1);
    assert(loot.count[DP_LOOT_IRON_INGOT] == 5);
    assert(loot.count[DP_LOOT_GOLD_INGOT] == 6);
    assert(loot.count[DP_LOOT_PRISMARINE_CRYSTALS] == 3);
    assert(loot.count[DP_LOOT_IRON_SWORD] == 1);
    assert(loot.count[DP_LOOT_COOKED_COD] == 7);

    assert(getRuinedPortalLoot16(&loot, 239648ULL, 4, 7));
    assert(loot.count[DP_LOOT_CLOCK] == 1);
    assert(loot.count[DP_LOOT_FLINT_AND_STEEL] == 1);
    assert(loot.count[DP_LOOT_GOLDEN_LEGGINGS] == 1);
    assert(loot.count[DP_LOOT_GOLDEN_SWORD] == 1);

    StructureLoot ship[SHIPWRECK_CHEST_COUNT];
    uint8_t present[SHIPWRECK_CHEST_COUNT];
    assert(getShipwreckLoot16(
        ship, present, 2276366175191987160ULL, -31, -32, 0));
    assert(present[SHIPWRECK_CHEST_SUPPLY]);
    assert(present[SHIPWRECK_CHEST_MAP]);
    assert(present[SHIPWRECK_CHEST_TREASURE]);
    assert(ship[SHIPWRECK_CHEST_SUPPLY].count[DP_LOOT_COAL] == 11);
    assert(ship[SHIPWRECK_CHEST_SUPPLY].count[DP_LOOT_PAPER] == 12);
    assert(ship[SHIPWRECK_CHEST_SUPPLY].count[
        DP_LOOT_POISONOUS_POTATO] == 2);
    assert(ship[SHIPWRECK_CHEST_SUPPLY].count[
        DP_LOOT_SUSPICIOUS_STEW] == 2);
    assert(ship[SHIPWRECK_CHEST_SUPPLY].count[DP_LOOT_WHEAT] == 18);
    assert(ship[SHIPWRECK_CHEST_MAP].count[DP_LOOT_FILLED_MAP] == 1);
    assert(ship[SHIPWRECK_CHEST_MAP].count[DP_LOOT_CLOCK] == 1);
    assert(ship[SHIPWRECK_CHEST_MAP].count[DP_LOOT_FEATHER] == 2);
    assert(ship[SHIPWRECK_CHEST_MAP].count[DP_LOOT_PAPER] == 6);
    assert(ship[SHIPWRECK_CHEST_TREASURE].count[
        DP_LOOT_IRON_INGOT] == 8);
    assert(ship[SHIPWRECK_CHEST_TREASURE].count[
        DP_LOOT_IRON_NUGGET] == 17);
}

static int floorDiv4ForTest(int value)
{
    int quotient = value / 4;
    if (value % 4 < 0)
        quotient--;
    return quotient;
}

static void assertSurfaceHeightColumnCache116(void)
{
    static const struct
    {
        int x, z, height;
    } cases[] = {
        {-410, 327, 66},
        {-399, 340, 66},
        {-405, 353, 66},
        {-386, 338, 65},
        {-421, 319, 66},
        {-400, 336, 64},
        {0, 0, 72},
        {100, 100, 63},
        {-1, -1, 71},
        {-4, -4, 70},
        {-5, -5, 70},
    };

    Generator generator;
    setupGenerator(&generator, MC_1_16_1, 0);
    applySeed(&generator, DIM_OVERWORLD, 0);
    SurfaceNoise noise;
    initSurfaceNoise(&noise, DIM_OVERWORLD, 0);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const int cellX = floorDiv4ForTest(cases[i].x);
        const int cellZ = floorDiv4ForTest(cases[i].z);
        double columns[4][33];
        assert(getTerrainNoiseColumn116(
            &generator, &noise, cellX, cellZ, columns[0]));
        assert(getTerrainNoiseColumn116(
            &generator, &noise, cellX, cellZ + 1, columns[1]));
        assert(getTerrainNoiseColumn116(
            &generator, &noise, cellX + 1, cellZ, columns[2]));
        assert(getTerrainNoiseColumn116(
            &generator, &noise, cellX + 1, cellZ + 1, columns[3]));
        assert(getFirstFreeHeightFromColumns116(
            (const double (*)[33])columns,
            cases[i].x, cases[i].z) == cases[i].height);
        assert(getFirstFreeHeight116(
            &generator, &noise,
            cases[i].x, cases[i].z) == cases[i].height);
    }
}

int main(void)
{
    static const char *stable[] = {
        "1.21.1", "1.21.2", "1.21.3", "1.21.4", "1.21.5",
        "1.21.6", "1.21.7", "1.21.8", "1.21.9", "1.21.10",
        "1.21.11", "26.1", "26.1.1", "26.1.2", "26.2",
    };

    for (int mc = MC_UNDEF + 1; mc <= MC_NEWEST; mc++)
        assert(strcmp(mc2str(mc), "?") != 0);

    for (size_t i = 0; i < sizeof(stable) / sizeof(stable[0]); i++)
    {
        int mc = str2mc(stable[i]);
        assert(mc != MC_UNDEF);
        assert(strcmp(mc2str(mc), stable[i]) == 0);
    }

    assert(str2mc("1.21 WD") == MC_1_21_4);
    assert(biomeExists(MC_1_21_3, pale_garden) == 0);
    assert(biomeExists(MC_1_21_4, pale_garden) == 1);
    assert(biomeExists(MC_26_1_2, sulfur_caves) == 0);
    assert(biomeExists(MC_26_2, sulfur_caves) == 1);
    assert(isViableFeatureBiome(MC_1_21_4, Mansion, pale_garden) == 0);
    assert(isViableFeatureBiome(MC_1_21_5, Mansion, pale_garden) == 1);

    static const int32_t expanded_pale_ranges[10][6] = {
        {3000, 10000, -7799, -3750, -10000, -9333},
        {3000, 10000, -3750, -2225, -10000, -9333},
        { 300, 10000, -3750, -2225,  -9333, -7666},
        {3000, 10000, -2225,   500,  -9333, -7666},
        { 300, 10000, -3750, -2225,  -7666, -5666},
        {3000, 10000, -2225,   500,  -7666, -5666},
        { 300, 10000, -3750, -2225,  -5666, -4000},
        {3000, 10000, -2225,   500,  -5666, -4000},
        {3000, 10000, -7799, -3750,  -4000, -2666},
        {3000, 10000, -3750, -2225,  -4000, -2666},
    };
    for (int i = 0; i < 10; i++)
    {
        for (int depth = 0; depth <= 10000; depth += 10000)
        {
            const int32_t *r = expanded_pale_ranges[i];
            uint64_t expanded_pale[6] = {
                250, 6500, (uint64_t)(int64_t)((r[0] + r[1]) / 2),
                (uint64_t)(int64_t)((r[2] + r[3]) / 2), depth,
                (uint64_t)(int64_t)((r[4] + r[5]) / 2),
            };
            assert(climateToBiome(MC_1_21_4, expanded_pale, NULL) == dark_forest);
            assert(climateToBiome(MC_1_21_5, expanded_pale, NULL) == pale_garden);
        }
    }

    // A point inside the stable 26.2 Sulfur Caves parameter box. Before 26.2
    // the same climate point must resolve to one of the pre-existing biomes.
    const uint64_t np[6] = {
        0, 0, 0, 5000, 5000, (uint64_t)(int64_t)-9000
    };
    assert(climateToBiome(MC_26_2, np, NULL) == sulfur_caves);
    assert(climateToBiome(MC_26_1_2, np, NULL) != sulfur_caves);

    const int *lim = getBiomeParaLimits(MC_26_2, sulfur_caves);
    assert(lim != NULL);
    assert(lim[4] == -1900 && lim[5] == 5500);
    assert(lim[6] == 4500 && lim[7] == INT_MAX);
    assert(lim[8] == 2000 && lim[9] == 9000);
    assert(lim[10] == -11000 && lim[11] == -8500);

    // Stable Java structure-set parameters from the official 26.2 data pack.
    // The values are semantically unchanged from 1.21.4 through 26.2.
    assertStructureConfig(Desert_Pyramid, MC_26_2, 32, 8, 14357617);
    assertStructureConfig(Igloo,          MC_26_2, 32, 8, 14357618);
    assertStructureConfig(Jungle_Pyramid, MC_26_2, 32, 8, 14357619);
    assertStructureConfig(Swamp_Hut,      MC_26_2, 32, 8, 14357620);
    assertStructureConfig(Village,        MC_26_2, 34, 8, 10387312);
    assertStructureConfig(Ocean_Ruin,     MC_26_2, 20, 8, 14357621);
    assertStructureConfig(Shipwreck,      MC_26_2, 24, 4, 165745295);
    assertStructureConfig(Monument,       MC_26_2, 32, 5, 10387313);
    assertStructureConfig(Mansion,        MC_26_2, 80, 20, 10387319);
    assertStructureConfig(Outpost,        MC_26_2, 32, 8, 165745296);
    assertStructureConfig(Ruined_Portal,  MC_26_2, 40, 15, 34222645);
    assertStructureConfig(Ruined_Portal_N, MC_26_2, 40, 15, 34222645);
    assertStructureConfig(Ancient_City,   MC_26_2, 24, 8, 20083232);
    assertStructureConfig(Trail_Ruins,    MC_26_2, 34, 8, 83469867);
    assertStructureConfig(Trial_Chambers, MC_26_2, 34, 12, 94251327);
    assertStructureConfig(Fortress,       MC_26_2, 27, 4, 30084232);
    assertStructureConfig(Bastion,        MC_26_2, 27, 4, 30084232);
    assertStructureConfig(End_City,       MC_26_2, 20, 11, 10387313);

    // Buried treasure and mineshafts use Mojang's legacy frequency reducers.
    // The treasure salt is supplied by legacy_type_2 rather than the JSON salt.
    assertStructureConfig(Treasure, MC_26_2, 1, 0, 10387320);
    assertStructureConfig(Mineshaft, MC_26_2, 1, 0, 0);

    // Starts observed in fully generated, unmodified Vanilla server worlds.
    // seed 8371904829, Java 26.2
    assertStructurePos(Ruined_Portal,  MC_26_2, 8371904829ULL,  0,   0,  272,   48);
    assertStructurePos(Treasure,       MC_26_2, 8371904829ULL,  0, -14,    9, -215);
    assertStructurePos(Mineshaft,      MC_26_2, 8371904829ULL, -14, 15, -224,  240);
    assertStructurePos(Trial_Chambers, MC_26_2, 8371904829ULL,  0,   0,  272,   96);

    // seed 3515201313347228787, Java 1.21.5. The mansion coordinate is the
    // exact result returned by Vanilla's /locate command.
    assertStructurePos(Mansion,        MC_1_21_5, 3515201313347228787ULL,
                        3, -1, 4320, -768);
    assertStructurePos(Ruined_Portal,  MC_1_21_5, 3515201313347228787ULL,
                        0, 0, 48, 176);
    assertStructurePos(Trial_Chambers, MC_1_21_5, 3515201313347228787ULL,
                        0, 0, 48, 240);

    Generator g;
    setupGenerator(&g, MC_1_21_5, 0);
    applySeed(&g, DIM_OVERWORLD, 3515201313347228787ULL);
    assert(isViableStructurePos(Mansion, &g, 4320, -768, 0));

    // 1.21.2 replaced the spawn fitness function. This seed exercises both
    // sides of the exact version boundary found in Mojang's Climate class.
    Generator spawnOld, spawnNew;
    setupGenerator(&spawnOld, MC_1_21_1, 0);
    setupGenerator(&spawnNew, MC_1_21_2, 0);
    applySeed(&spawnOld, DIM_OVERWORLD, 7636401805092394055ULL);
    applySeed(&spawnNew, DIM_OVERWORLD, 7636401805092394055ULL);
    Pos oldSpawn = estimateSpawn(&spawnOld, NULL);
    Pos newSpawn = estimateSpawn(&spawnNew, NULL);
    assert(oldSpawn.x == -136 && oldSpawn.z == 584);
    assert(newSpawn.x == -760 && newSpawn.z == -920);

    assertStructureVariants();
    assertDesertPyramidLoot();
    assertAdditionalStructureLoot();
    assertVillageBastionLootTables();
    assertSurfaceHeightColumnCache116();

    puts("stable version, biome, and structure tests passed");
    return 0;
}
