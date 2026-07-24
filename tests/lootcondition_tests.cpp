#include "src/search.h"

#include <QCoreApplication>

#include <assert.h>
#include <stdio.h>

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

int main(int argc, char **argv)
{
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
    assert(matchStructureLoot(rules, MC_1_16_1, seed, pyramid));

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
    QString testArgument = argc > 1
        ? QString::fromLocal8Bit(argv[1]) : QString();
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

    puts("loot condition tests passed");
    return 0;
}
