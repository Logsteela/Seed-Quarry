#include "src/search.h"
#include "src/villagelootseed.h"
#include "src/villagestructure.h"

#include <QCoreApplication>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/*
 * The CRT assert handler can open a modal "unknown software exception"
 * dialog on Windows.  These are command-line tests, so report the failed
 * expression in the terminal and return a normal non-zero exit code instead.
 */
static void testAssertionFailed(
    const char *expression, const char *file, int line)
{
    fprintf(
        stderr, "CHECK failed: %s (%s:%d)\n",
        expression, file, line);
    fflush(stderr);
    exit(3);
}

#undef assert
#define assert(expression) \
    ((expression) ? (void)0 : \
        testAssertionFailed(#expression, __FILE__, __LINE__))

extern "C" int getStructureConfig_override(
    int stype, int mc, StructureConfig *sconf)
{
    return getStructureConfig(stype, mc, sconf);
}

static LootRule itemRule(int item, int minimum, int maximum)
{
    LootRule rule;
    rule.item = item;
    rule.minCount = minimum;
    rule.maxCount = maximum;
    return rule;
}

struct GeneratedLootCase
{
    int structureType = 0;
    int conditionType = 0;
    Pos pos = {};
    int biomeId = -1;
    int chestMode = LootRuleSet::CHESTS_TOTAL;
    LootRule rule;
};

static bool firstPresentItem(
    const StructureLoot& loot, LootRule *rule)
{
    for (int item = 0; item < DP_LOOT_ITEM_COUNT; item++)
    {
        if (loot.count[item] > 0)
        {
            *rule = itemRule(item, 1, -1);
            return true;
        }
    }
    return false;
}

static GeneratedLootCase findGeneratedLootCase(
    int structureType, int conditionType, uint64_t seed,
    Generator *generator)
{
    GeneratedLootCase result;
    result.structureType = structureType;
    result.conditionType = conditionType;

    for (int regionZ = -64; regionZ <= 64; regionZ++)
    {
        for (int regionX = -64; regionX <= 64; regionX++)
        {
            Pos pos;
            if (!getStructurePos(
                    structureType, MC_1_16_1, seed,
                    regionX, regionZ, &pos) ||
                !isViableStructurePos(
                    structureType, generator, pos.x, pos.z, 0))
            {
                continue;
            }

            if (structureType == Treasure)
            {
                result.pos = pos;
                result.rule =
                    itemRule(DP_LOOT_HEART_OF_THE_SEA, 1, 1);
                return result;
            }

            if (structureType == Ruined_Portal)
            {
                StructureLoot loot = {};
                if (getRuinedPortalLoot16(
                        &loot, seed, pos.x >> 4, pos.z >> 4) &&
                    firstPresentItem(loot, &result.rule))
                {
                    result.pos = pos;
                    return result;
                }
                continue;
            }

            if (structureType == Shipwreck)
            {
                const int chunkX = pos.x >> 4;
                const int chunkZ = pos.z >> 4;
                const int biomeId = getBiomeAt(
                    generator, 4, chunkX * 4 + 2, 0,
                    chunkZ * 4 + 2);
                StructureLoot loot[SHIPWRECK_CHEST_COUNT] = {};
                uint8_t present[SHIPWRECK_CHEST_COUNT] = {};
                const int beached =
                    biomeId == beach || biomeId == snowy_beach;
                if (!getShipwreckLoot16(
                        loot, present, seed, chunkX, chunkZ,
                        beached))
                {
                    continue;
                }
                for (int chest = 0;
                     chest < SHIPWRECK_CHEST_COUNT; chest++)
                {
                    if (present[chest] &&
                        firstPresentItem(
                            loot[chest], &result.rule))
                    {
                        result.pos = pos;
                        result.biomeId = biomeId;
                        result.chestMode =
                            LootRuleSet::CHEST_1 + chest;
                        return result;
                    }
                }
            }
        }
    }

    assert(!"generated structure with loot was not found");
    return result;
}

static void printConditionHex(
    const GeneratedLootCase& generated, int mc)
{
    Condition condition = {};
    condition.type = generated.conditionType;
    condition.x1 = condition.x2 = generated.pos.x;
    condition.z1 = condition.z2 = generated.pos.z;
    condition.save = 1;
    condition.count = 1;
    condition.version = Condition::VER_CURRENT;
    condition.flags = Condition::FLG_LOOT;
    QByteArray base(
        reinterpret_cast<const char*>(&condition),
        offsetof(Condition, generated_start));

    LootRuleSet rules;
    rules.structureType = generated.structureType;
    rules.logic = LootRuleSet::LOGIC_ALL;
    rules.instanceMode = LootRuleSet::INSTANCE_ANY;
    rules.chestMode = generated.chestMode;
    rules.rules << generated.rule;
    assert(validateLootRuleSet(rules, mc).isEmpty());

    QByteArray payload =
        serializeLootRuleSet(rules).toBase64(
            QByteArray::Base64UrlEncoding |
            QByteArray::OmitTrailingEquals);
    printf("condition=%s|%s\n",
           base.toHex().constData(), payload.constData());
}

int main(int argc, char **argv)
{
#ifdef Q_OS_WIN
    // Keep failed assertions in the invoking terminal instead of opening a
    // modal Windows crash dialog on the user's desktop.
    SetErrorMode(
        SEM_FAILCRITICALERRORS |
        SEM_NOGPFAULTERRORBOX |
        SEM_NOOPENFILEERRORBOX);
#endif

    static_assert(LootRuleSet::CHESTS_TOTAL == 0, "saved chest mode changed");
    static_assert(LootRuleSet::CHEST_ANY == 1, "saved chest mode changed");
    static_assert(LootRuleSet::CHEST_EVERY == 2, "saved chest mode changed");
    static_assert(LootRuleSet::CHEST_1 == 3, "saved chest mode changed");
    static_assert(LootRuleSet::CHEST_4 == 6, "saved chest mode changed");

    QCoreApplication app(argc, argv);
    const uint64_t seed = UINT64_C(3515201313347228787);
    const Pos pyramid = {17 * 16, -9 * 16};

    LootRuleSet rules;
    rules.structureType = Desert_Pyramid;
    rules.logic = LootRuleSet::LOGIC_ALL;
    rules.instanceMode = LootRuleSet::INSTANCE_ANY;
    rules.chestMode = LootRuleSet::CHESTS_TOTAL;
    rules.rules << itemRule(DP_LOOT_DIAMOND, 1, 1);
    assert(validateLootRuleSet(rules, MC_1_16_1).isEmpty());
    assert(serializeLootRuleSet(rules).toHex() ==
        QByteArray(
            "534c52310100010000000001000000000001000000"
            "01000000ffff01000500"));
    assert(matchStructureLoot(rules, MC_1_16_1, seed, pyramid));

    LootSearchCacheEntry variableChestEntry;
    variableChestEntry.chests.resize(7);
    for (int i = 0; i < variableChestEntry.chests.size(); i++)
    {
        variableChestEntry.chests[i].present = i % 2 == 0;
        variableChestEntry.chests[i].counts.push_back(uint64_t(i));
    }
    assert(variableChestEntry.chests.size() == 7);
    assert(variableChestEntry.chests[6].present);
    assert(variableChestEntry.chests[6].counts[0] == 6);

    LootRuleSet allChests = rules;
    allChests.rules[0] = itemRule(DP_LOOT_DIAMOND, 0, -1);
    allChests.chestMode = LootRuleSet::CHEST_ANY;
    assert(matchStructureLoot(
        allChests, MC_1_16_1, seed, pyramid));
    allChests.chestMode = LootRuleSet::CHEST_EVERY;
    assert(matchStructureLoot(
        allChests, MC_1_16_1, seed, pyramid));

    LootRuleSet positionOnly = rules;
    positionOnly.rules[0] =
        itemRule(DP_LOOT_ANY_CONTAINER, 4, 4);
    assert(matchStructureLoot(
        positionOnly, MC_1_16_1, seed, pyramid));

    VillageLayout16 villageLayout;
    QString villageError;
    assert(generateVillageLayout16(
        &villageLayout, 0, -25, 21, taiga,
        &villageError));
    assert(villageLayout.pieceCount == 62);
    QVector<VillageLootChestSeed16> villageChests;
    assert(assignVillageLootSeedsSingleStart16(
        &villageChests, villageLayout, 0, false,
        &villageError));
    assert(villageChests.size() == 2);
    assert(villageChests[0].isExact());

    LootRuleSet villagePosition;
    villagePosition.structureType = Village;
    villagePosition.logic = LootRuleSet::LOGIC_ALL;
    villagePosition.instanceMode = LootRuleSet::INSTANCE_ANY;
    villagePosition.chestMode = LootRuleSet::CHESTS_TOTAL;
    villagePosition.rules <<
        itemRule(DP_LOOT_ANY_CONTAINER, 1, 1);
    villagePosition.chestPositionMode =
        LootRuleSet::CHEST_POSITION_ABSOLUTE;
    villagePosition.chestMinX =
        villagePosition.chestMaxX =
            villageChests[0].container.pos.x;
    villagePosition.chestMinY =
        villagePosition.chestMaxY =
            villageChests[0].container.pos.y;
    villagePosition.chestMinZ =
        villagePosition.chestMaxZ =
            villageChests[0].container.pos.z;
    assert(validateLootRuleSet(
        villagePosition, MC_1_16_1).isEmpty());
    assert(matchStructureLootStatus(
        villagePosition, MC_1_16_1, 0,
        Pos{-25 * 16, 21 * 16}, taiga) ==
        LOOT_MATCH_YES);

    const Pos villageReference = {
        villageChests[0].container.pos.x - 7,
        villageChests[0].container.pos.z + 9,
    };
    villagePosition.chestPositionMode =
        LootRuleSet::CHEST_POSITION_LOCATION_REFERENCE;
    villagePosition.chestMinX =
        villagePosition.chestMaxX = 7;
    villagePosition.chestMinY =
        villagePosition.chestMaxY =
            villageChests[0].container.pos.y;
    villagePosition.chestMinZ =
        villagePosition.chestMaxZ = -9;
    assert(validateLootRuleSet(
        villagePosition, MC_1_16_1).isEmpty());
    assert(matchStructureLootStatus(
        villagePosition, MC_1_16_1, 0,
        Pos{-25 * 16, 21 * 16}, taiga,
        nullptr, 0, villageReference) ==
        LOOT_MATCH_YES);
    assert(matchStructureLootStatus(
        villagePosition, MC_1_16_1, 0,
        Pos{-25 * 16, 21 * 16}, taiga,
        nullptr, 0, Pos{0, 0}) ==
        LOOT_MATCH_NO);
    LootRuleSet decodedVillagePosition;
    assert(deserializeLootRuleSet(
        serializeLootRuleSet(villagePosition),
        &decodedVillagePosition, &villageError));
    assert(decodedVillagePosition.chestPositionMode ==
        LootRuleSet::CHEST_POSITION_LOCATION_REFERENCE);

    LootRuleSet rejectedVillageLoot = villagePosition;
    rejectedVillageLoot.chestPositionMode =
        LootRuleSet::CHEST_POSITION_ANY;
    rejectedVillageLoot.rules[0] =
        itemRule(DP_LOOT_ANY_CONTAINER, 999, -1);
    Condition villageLootCondition = {};
    villageLootCondition.type = F_VILLAGE;
    villageLootCondition.x1 =
        villageLootCondition.x2 = -25 * 16;
    villageLootCondition.z1 =
        villageLootCondition.z2 = 21 * 16;
    villageLootCondition.save = 1;
    villageLootCondition.count = 1;
    villageLootCondition.version = Condition::VER_CURRENT;
    villageLootCondition.flags = Condition::FLG_LOOT;
    villageLootCondition.hash =
        registerLootRuleSet(rejectedVillageLoot);

    Generator generator;
    setupGenerator(&generator, MC_1_16_1, 0);
    applySeed(&generator, DIM_OVERWORLD, seed);
    Pos generatedPyramid = {};
    bool foundGeneratedDiamond = false;
    for (int regionZ = -32;
         regionZ <= 32 && !foundGeneratedDiamond; regionZ++)
    {
        for (int regionX = -32;
             regionX <= 32 && !foundGeneratedDiamond; regionX++)
        {
            Pos pos;
            if (getStructurePos(
                    Desert_Pyramid, MC_1_16_1, seed,
                    regionX, regionZ, &pos) &&
                isViableStructurePos(
                    Desert_Pyramid, &generator, pos.x, pos.z, 0) &&
                matchStructureLoot(
                    rules, MC_1_16_1, seed, pos))
            {
                generatedPyramid = pos;
                foundGeneratedDiamond = true;
            }
        }
    }
    assert(foundGeneratedDiamond);
    assert(matchStructureLoot(
        rules, MC_1_16_1, seed, generatedPyramid));
    printf("generated diamond pyramid at %d,%d\n",
           generatedPyramid.x, generatedPyramid.z);

    const GeneratedLootCase generatedShipwreck =
        findGeneratedLootCase(
            Shipwreck, F_SHIPWRECK, seed, &generator);
    const GeneratedLootCase generatedTreasure =
        findGeneratedLootCase(
            Treasure, F_TREASURE, seed, &generator);
    const GeneratedLootCase generatedPortal =
        findGeneratedLootCase(
            Ruined_Portal, F_PORTAL, seed, &generator);
    assert(matchStructureLoot(
        LootRuleSet{
            generatedShipwreck.structureType,
            LootRuleSet::LOGIC_ALL,
            LootRuleSet::INSTANCE_ANY,
            generatedShipwreck.chestMode,
            {generatedShipwreck.rule}},
        MC_1_16_1, seed, generatedShipwreck.pos,
        generatedShipwreck.biomeId));

    QString testArgument = argc > 1
        ? QString::fromLocal8Bit(argv[1]) : QString();
    if (testArgument == "--shipwreck-condition-hex")
        printConditionHex(generatedShipwreck, MC_1_16_1);
    else if (testArgument == "--treasure-condition-hex")
        printConditionHex(generatedTreasure, MC_1_16_1);
    else if (testArgument == "--portal-condition-hex")
        printConditionHex(generatedPortal, MC_1_16_1);
    else if (testArgument ==
             "--village-family-condition-hex")
    {
        QByteArray base(
            reinterpret_cast<const char*>(
                &villageLootCondition),
            offsetof(Condition, generated_start));
        QByteArray payload =
            serializeLootRuleSet(
                rejectedVillageLoot).toBase64(
                QByteArray::Base64UrlEncoding |
                QByteArray::OmitTrailingEquals);
        printf("condition=%s|%s\n",
               base.toHex().constData(),
               payload.constData());
    }
    if (testArgument == "--condition-hex" ||
        testArgument == "--condition-hex-fail" ||
        testArgument == "--structure-condition-hex")
    {
        Condition condition = {};
        condition.type = testArgument == "--structure-condition-hex"
            ? F_DESERT : F_LOOT;
        condition.x1 = condition.x2 = generatedPyramid.x;
        condition.z1 = condition.z2 = generatedPyramid.z;
        condition.save = 1;
        condition.count = 1;
        condition.version = Condition::VER_CURRENT;
        condition.flags = Condition::FLG_LOOT;
        QByteArray base(
            reinterpret_cast<const char*>(&condition),
            offsetof(Condition, generated_start));
        LootRuleSet integrationRules;
        integrationRules.structureType = Desert_Pyramid;
        integrationRules.logic = LootRuleSet::LOGIC_ALL;
        integrationRules.instanceMode =
            testArgument == "--structure-condition-hex"
                ? LootRuleSet::INSTANCE_ANY
                : LootRuleSet::INSTANCE_TOTAL;
        integrationRules.chestMode = LootRuleSet::CHESTS_TOTAL;
        integrationRules.rules <<
            itemRule(
                DP_LOOT_DIAMOND,
                testArgument == "--condition-hex-fail" ? 999 : 1,
                -1);
        QByteArray payload =
            serializeLootRuleSet(integrationRules).toBase64(
                QByteArray::Base64UrlEncoding |
                QByteArray::OmitTrailingEquals);
        printf("condition=%s|%s\n",
               base.toHex().constData(), payload.constData());
    }

    rules.rules[0].minCount = 2;
    assert(!matchStructureLoot(rules, MC_1_16_5, seed, pyramid));

    LootRule silk = itemRule(DP_LOOT_ENCHANTED_BOOK, 1, 1);
    silk.enchantment = DP_ENCH_SILK_TOUCH;
    silk.minLevel = silk.maxLevel = 1;
    rules.rules[0] = silk;
    rules.chestMode = LootRuleSet::CHEST_4;
    assert(matchStructureLoot(rules, MC_1_16_1, seed, pyramid));

    rules.rules[0].enchantment = DP_ENCH_MENDING;
    assert(!matchStructureLoot(rules, MC_1_16_1, seed, pyramid));

    rules.chestMode = LootRuleSet::CHESTS_TOTAL;
    rules.logic = LootRuleSet::LOGIC_ANY;
    rules.rules.clear();
    rules.rules << itemRule(DP_LOOT_DIAMOND, 99, -1) << silk;
    assert(matchStructureLoot(rules, MC_1_16_1, seed, pyramid));

    rules.logic = LootRuleSet::LOGIC_ALL;
    rules.rules.clear();
    rules.rules << itemRule(DP_LOOT_DIAMOND, 2, 2);
    rules.instanceMode = LootRuleSet::INSTANCE_TOTAL;
    QVector<Pos> positions;
    positions << pyramid << pyramid;
    assert(matchAreaLoot(rules, MC_1_16_1, seed, positions));

    LootRuleSet additional;
    additional.logic = LootRuleSet::LOGIC_ALL;
    additional.instanceMode = LootRuleSet::INSTANCE_ANY;
    additional.chestMode = LootRuleSet::CHESTS_TOTAL;

    additional.structureType = Treasure;
    additional.rules << itemRule(
        DP_LOOT_HEART_OF_THE_SEA, 1, 1);
    assert(matchStructureLoot(
        additional, MC_1_16_5, 123, Pos{905, -1671}));

    additional.structureType = Ruined_Portal;
    additional.rules[0] = itemRule(DP_LOOT_CLOCK, 1, 1);
    assert(matchStructureLoot(
        additional, MC_1_16_5, 239648, Pos{64, 112}));

    additional.structureType = Shipwreck;
    additional.chestMode = LootRuleSet::CHEST_2;
    additional.rules[0] = itemRule(DP_LOOT_FILLED_MAP, 1, 1);
    assert(matchStructureLoot(
        additional, MC_1_16_5,
        UINT64_C(2276366175191987160),
        Pos{-31 * 16, -32 * 16}, ocean));
    additional.chestMode = LootRuleSet::CHEST_1;
    additional.rules[0] = itemRule(DP_LOOT_WHEAT, 18, 18);
    assert(matchStructureLoot(
        additional, MC_1_16_5,
        UINT64_C(2276366175191987160),
        Pos{-31 * 16, -32 * 16}, ocean));

    LootSearchCache familyCache;
    const uint64_t shipSeed =
        UINT64_C(2276366175191987160);
    const uint64_t sameFamilySeed =
        ((shipSeed + (UINT64_C(1) << 48)) & ~MASK48) |
        (shipSeed & MASK48);
    assert(matchStructureLoot(
        additional, MC_1_16_5, shipSeed,
        Pos{-31 * 16, -32 * 16}, ocean, &familyCache));
    assert(matchStructureLoot(
        additional, MC_1_16_5, sameFamilySeed,
        Pos{-31 * 16, -32 * 16}, ocean, &familyCache));
    assert(familyCache.calculations == 1);
    assert(familyCache.hits == 1);

    // Beached and ocean shipwrecks use different template pools and RNG
    // advancement, so they deliberately occupy separate cache entries.
    (void) matchStructureLoot(
        additional, MC_1_16_5, sameFamilySeed,
        Pos{-31 * 16, -32 * 16}, beach, &familyCache);
    assert(familyCache.calculations == 2);
    (void) matchStructureLoot(
        additional, MC_1_16_5, shipSeed,
        Pos{-31 * 16, -32 * 16}, beach, &familyCache);
    assert(familyCache.hits == 2);

    QVector<Pos> repeatedShips{
        Pos{-31 * 16, -32 * 16},
        Pos{-31 * 16, -32 * 16},
    };
    QVector<int> repeatedBiomes{ocean, ocean};
    familyCache.reset();
    (void) matchAreaLoot(
        additional, MC_1_16_5, shipSeed,
        repeatedShips, repeatedBiomes, &familyCache);
    assert(familyCache.calculations == 1);
    assert(familyCache.hits == 1);

    bool oceanShipMatch = matchStructureLoot(
        additional, MC_1_16_5, shipSeed,
        Pos{-31 * 16, -32 * 16}, ocean);
    bool beachedShipMatch = matchStructureLoot(
        additional, MC_1_16_5, shipSeed,
        Pos{-31 * 16, -32 * 16}, beach);
    familyCache.reset();
    assert(canMatchStructureLoot48(
        additional, MC_1_16_5, shipSeed,
        Pos{-31 * 16, -32 * 16}, &familyCache) ==
        (oceanShipMatch || beachedShipMatch));
    assert(familyCache.calculations == 2);

    LootRuleSet possibleTotal;
    possibleTotal.structureType = Desert_Pyramid;
    possibleTotal.logic = LootRuleSet::LOGIC_ALL;
    possibleTotal.instanceMode = LootRuleSet::INSTANCE_TOTAL;
    possibleTotal.chestMode = LootRuleSet::CHESTS_TOTAL;
    possibleTotal.rules << itemRule(DP_LOOT_DIAMOND, 2, 2);
    QVector<Pos> candidatePyramids{pyramid, pyramid};
    familyCache.reset();
    assert(canMatchAreaLoot48(
        possibleTotal, MC_1_16_1, seed,
        candidatePyramids, 2, &familyCache));
    possibleTotal.rules[0] =
        itemRule(DP_LOOT_DIAMOND, 3, -1);
    familyCache.reset();
    assert(!canMatchAreaLoot48(
        possibleTotal, MC_1_16_1, seed,
        candidatePyramids, 2, &familyCache));
    possibleTotal.rules[0] =
        itemRule(DP_LOOT_DIAMOND, 0, 1);
    familyCache.reset();
    assert(!canMatchAreaLoot48(
        possibleTotal, MC_1_16_1, seed,
        candidatePyramids, 2, &familyCache));

    LootRuleSet wideRules = rules;
    wideRules.rules.clear();
    wideRules.rules << itemRule(DP_LOOT_DIAMOND, 70000, 70000);
    QVector<Pos> widePositions;
    widePositions.fill(pyramid, 70000);
    assert(matchAreaLoot(
        wideRules, MC_1_16_1, seed, widePositions));

    for (int i = 0; i < 5000; i++)
        rules.rules << itemRule(i % DP_LOOT_ITEM_COUNT, 0, -1);
    QByteArray encoded = serializeLootRuleSet(rules);
    LootRuleSet decoded;
    QString error;
    assert(deserializeLootRuleSet(encoded, &decoded, &error));
    assert(decoded.rules.size() == rules.rules.size());
    assert(serializeLootRuleSet(decoded) == encoded);

    uint64_t hash = registerLootRuleSet(decoded);
    LootRuleSet stored;
    assert(hash != 0 && lookupLootRuleSet(hash, &stored));
    assert(serializeLootRuleSet(stored) == encoded);
    assert(!isLootSupported(Desert_Pyramid, MC_1_20));
    assert(isLootSupported(Shipwreck, MC_1_16_5));
    assert(isLootSupported(Treasure, MC_1_16_1));
    assert(isLootSupported(Ruined_Portal, MC_1_16_5));
    assert(isLootSupported(Ruined_Portal_N, MC_1_16_1));
    assert(isLootSupported(Village, MC_1_16_1));
    assert(isLootSupported(Village, MC_1_16));
    assert(!isLootSupported(Village, MC_1_17));
    assert(structureLootItemAvailable(
        Shipwreck, DP_LOOT_FILLED_MAP));
    assert(!structureLootItemAvailable(
        Treasure, DP_LOOT_FILLED_MAP));

    puts("loot condition tests passed");
    return 0;
}
