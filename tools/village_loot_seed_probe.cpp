#include "src/villagelootseed.h"
#include "src/villagestructure.h"

#include <QCoreApplication>
#include <QTextStream>

extern "C" int getStructureConfig_override(
    int structureType, int mc, StructureConfig *config)
{
    return getStructureConfig(structureType, mc, config);
}

namespace {

bool testFeatureQuality(
    QTextStream *errors, const QString& feature,
    int expectedQuality)
{
    VillageLayout16 layout;
    VillagePiece16 featurePiece;
    featurePiece.elementType = VillagePiece16::FEATURE;
    featurePiece.feature = feature;
    featurePiece.pos = {1, 64, 1};
    featurePiece.bb0 = featurePiece.pos;
    featurePiece.bb1 = featurePiece.pos;
    layout.pieces.push_back(featurePiece);

    VillagePiece16 templatePiece;
    templatePiece.elementType =
        VillagePiece16::LEGACY_TEMPLATE;
    templatePiece.bb0 = {0, 64, 0};
    templatePiece.bb1 = {15, 70, 15};
    layout.pieces.push_back(templatePiece);

    VillageContainer16 ordinary;
    ordinary.pos = {2, 65, 2};
    ordinary.block = QStringLiteral("barrel");
    ordinary.pieceIndex = 1;
    ordinary.placementIndex = 10;
    layout.containers.push_back(ordinary);

    VillageContainer16 loot = ordinary;
    loot.pos = {3, 65, 3};
    loot.lootTable =
        QStringLiteral("chests/village/village_plains_house");
    loot.table = 0;
    loot.placementIndex = 20;
    layout.containers.push_back(loot);

    QString error;
    QVector<VillageLootChestSeed16> chests;
    if (!assignVillageLootSeedsSingleStart16(
            &chests, layout, 1, false, &error))
    {
        *errors << error << '\n';
        return false;
    }
    if (chests.size() != 1 ||
        chests.first().quality != expectedQuality)
    {
        *errors << "Unexpected feature quality for "
                << feature << '\n';
        return false;
    }
    return true;
}

bool runCase(
    QTextStream *output, QTextStream *errors,
    uint64_t seed, int chunkX, int chunkZ, int biome,
    int expectedPieces, int expectedLootChests)
{
    QString error;
    VillageLayout16 layout;
    if (!generateVillageLayout16(
            &layout, seed, chunkX, chunkZ, biome, &error))
    {
        *errors << error << '\n';
        return false;
    }
    if (layout.pieceCount != expectedPieces)
    {
        *errors << "Unexpected piece count: "
                << layout.pieceCount << '\n';
        return false;
    }

    QVector<VillageLootChestSeed16> chests;
    if (!assignVillageLootSeedsSingleStart16(
            &chests, layout, seed, false, &error))
    {
        *errors << error << '\n';
        return false;
    }
    if (chests.size() != expectedLootChests)
    {
        *errors << "Unexpected loot chest count: "
                << chests.size() << '\n';
        return false;
    }
    for (const VillageLootChestSeed16& chest : chests)
    {
        if (!chest.isExact())
        {
            *errors
                << "Known vector contains an unresolved chest at "
                << chest.container.pos.x << ','
                << chest.container.pos.y << ','
                << chest.container.pos.z << '\n';
            return false;
        }
        *output
            << "CHEST\tseed=" << qint64(seed)
            << "\tpos=" << chest.container.pos.x << ','
            << chest.container.pos.y << ','
            << chest.container.pos.z
            << "\tloot_seed="
            << qint64(chest.lootTableSeed)
            << "\tpiece=" << chest.container.piece
            << "\tquality=EXACT\n";
    }

    QVector<VillageLootChestSeed16> overlap;
    if (!assignVillageLootSeedsSingleStart16(
            &overlap, layout, seed, true, &error))
    {
        *errors << error << '\n';
        return false;
    }
    for (const VillageLootChestSeed16& chest : overlap)
    {
        if (chest.quality !=
                VILLAGE_LOOT_SEED_UNRESOLVED_OVERLAP ||
            chest.lootTableSeed != 0)
        {
            *errors << "Overlap flag did not mark a chest.\n";
            return false;
        }
    }
    return true;
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream errors(stderr);

    if (argc != 1)
    {
        errors << "Usage: village_loot_seed_probe\n";
        return 1;
    }
    if (!runCase(
            &output, &errors, 0, -25, 21, taiga,
            62, 2))
    {
        return 2;
    }
    if (!runCase(
            &output, &errors, UINT64_C(8040347553),
            9, 20, savanna, 140, 4))
    {
        return 3;
    }
    if (!testFeatureQuality(
            &errors,
            QStringLiteral(
                "Feature.RANDOM_PATCH.configured("
                "BiomeDefaultFeatures.SWEET_BERRY_BUSH_CONFIG)"),
            VILLAGE_LOOT_SEED_EXACT) ||
        !testFeatureQuality(
            &errors,
            QStringLiteral(
                "Feature.RANDOM_PATCH.configured("
                "BiomeDefaultFeatures.TAIGA_GRASS_CONFIG)"),
            VILLAGE_LOOT_SEED_EXACT) ||
        !testFeatureQuality(
            &errors,
            QStringLiteral(
                "Feature.FLOWER.configured("
                "BiomeDefaultFeatures.PLAIN_FLOWER_CONFIG)"),
            VILLAGE_LOOT_SEED_EXACT) ||
        !testFeatureQuality(
            &errors,
            QStringLiteral(
                "Feature.TREE.configured("
                "BiomeDefaultFeatures.NORMAL_TREE_CONFIG)"),
            VILLAGE_LOOT_SEED_UNRESOLVED_FEATURE))
    {
        return 4;
    }
    output << "SELF_TEST\tok\n";
    return 0;
}
