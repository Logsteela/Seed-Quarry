#include "src/villagelootseed.h"
#include "src/villagestructure.h"

#include <QCoreApplication>
#include <QHash>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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
                << feature << ": count=" << chests.size()
                << ", quality="
                << (chests.isEmpty() ? -1 : chests.first().quality)
                << ", expected=" << expectedQuality << '\n';
        return false;
    }
    return true;
}

qint64 horizontalKey(int x, int z)
{
    return qint64(
        (quint64(quint32(x)) << 32) |
        quint64(quint32(z)));
}

bool testFlatFeatureSeed(
    QTextStream *errors, const QString& feature,
    int villageType, qint64 expectedSeed)
{
    VillageLayout16 layout;
    layout.villageType = villageType;
    VillagePiece16 featurePiece;
    featurePiece.elementType = VillagePiece16::FEATURE;
    featurePiece.feature = feature;
    featurePiece.pos = {8, 64, 8};
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

    for (int z = 0; z < 16; z++)
    {
        for (int x = 0; x < 16; x++)
        {
            layout.featureSurfaceHeights.insert(
                horizontalKey(x, z), 64);
        }
    }

    QVector<VillageLootChestSeed16> chests;
    QString error;
    if (!assignVillageLootSeedsSingleStart16(
            &chests, layout, 1, false, &error))
    {
        *errors << error << '\n';
        return false;
    }
    if (chests.size() != 1 ||
        !chests.first().isExact() ||
        qint64(chests.first().lootTableSeed) != expectedSeed)
    {
        *errors << "Flat feature seed mismatch for "
                << feature << ": got "
                << (chests.isEmpty()
                    ? QStringLiteral("no chest")
                    : QString::number(
                          qint64(chests.first().lootTableSeed)))
                << ", expected " << expectedSeed << '\n';
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

    if (chests.size() >= 2)
    {
        const int overlapChunkX =
            floordiv(chests.first().container.pos.x, 16);
        const int overlapChunkZ =
            floordiv(chests.first().container.pos.z, 16);
        QVector<Pos> overlapChunks{
            Pos{overlapChunkX, overlapChunkZ}};
        QVector<VillageLootChestSeed16> partialOverlap;
        if (!assignVillageLootSeedsSingleStart16(
                &partialOverlap, layout, seed,
                overlapChunks, &error))
        {
            *errors << error << '\n';
            return false;
        }
        for (int i = 0; i < partialOverlap.size(); i++)
        {
            const VillageLootChestSeed16& actual =
                partialOverlap[i];
            const bool inOverlapChunk =
                floordiv(actual.container.pos.x, 16) ==
                    overlapChunkX &&
                floordiv(actual.container.pos.z, 16) ==
                    overlapChunkZ;
            if (inOverlapChunk)
            {
                if (actual.quality !=
                        VILLAGE_LOOT_SEED_UNRESOLVED_OVERLAP ||
                    actual.lootTableSeed != 0)
                {
                    *errors
                        << "Per-chunk overlap did not mark "
                           "the selected chunk.\n";
                    return false;
                }
            }
            else if (!actual.isExact() ||
                     actual.lootTableSeed !=
                         chests[i].lootTableSeed)
            {
                *errors
                    << "Per-chunk overlap changed an "
                       "unaffected chunk.\n";
                return false;
            }
        }
    }
    return true;
}

int blockChunk(int coordinate)
{
    return floordiv(coordinate, 16);
}

bool featureBeforeContainerInChunk(
    const VillageLayout16& layout,
    const VillageContainer16& container,
    const VillagePiece16& feature)
{
    const int chunkX = blockChunk(container.pos.x);
    const int chunkZ = blockChunk(container.pos.z);
    const int minX = chunkX * 16;
    const int minZ = chunkZ * 16;
    return feature.elementType == VillagePiece16::FEATURE &&
        feature.pos.x >= minX && feature.pos.x <= minX + 15 &&
        feature.pos.z >= minZ && feature.pos.z <= minZ + 15 &&
        (&feature - layout.pieces.constData()) <
            container.pieceIndex;
}

bool scanUnresolved(
    QTextStream *output, QTextStream *errors, int count)
{
    static const int biomes[] = {
        plains, desert, savanna, snowy_tundra, taiga,
    };
    QHash<QString, int> featureCounts;
    QHash<QString, int> reasonCounts;
    QHash<QString, int> featureReasonCounts;
    int unresolvedChests = 0;
    int totalChests = 0;
    for (int index = 0; index < count; index++)
    {
        const uint64_t seed =
            UINT64_C(0x9e3779b97f4a7c15) *
            uint64_t(index + 1);
        const int chunkX = (index % 127) - 63;
        const int chunkZ = ((index * 37) % 127) - 63;
        VillageLayout16 layout;
        QString error;
        if (!generateVillageLayout16(
                &layout, seed, chunkX, chunkZ,
                biomes[index % 5], &error))
        {
            *errors << error << '\n';
            return false;
        }
        QVector<VillageLootChestSeed16> chests;
        if (!assignVillageLootSeedsSingleStart16(
                &chests, layout, seed, false, &error))
        {
            *errors << error << '\n';
            return false;
        }
        totalChests += chests.size();
        for (const VillageLootChestSeed16& chest : chests)
        {
            if (chest.isExact())
                continue;
            unresolvedChests++;
            reasonCounts[QString::fromLatin1(
                villageLootUnresolvedReasonName16(
                    chest.unresolvedReason))]++;
            if (chest.unresolvedFeatureIndex >= 0 &&
                chest.unresolvedFeatureIndex < layout.pieces.size())
            {
                const QString feature = layout.pieces[
                    chest.unresolvedFeatureIndex].feature;
                const QString reason = QString::fromLatin1(
                    villageLootUnresolvedReasonName16(
                        chest.unresolvedReason));
                featureCounts[feature]++;
                featureReasonCounts[reason + QLatin1Char('|') +
                    feature]++;
            }
        }
    }
    *output << "SCAN\tlayouts=" << count
            << "\tloot_chests=" << totalChests
            << "\tunresolved_chests=" << unresolvedChests
            << '\n';
    for (auto it = featureCounts.constBegin();
         it != featureCounts.constEnd(); ++it)
    {
        *output << "FEATURE\tchest_occurrences=" << it.value()
                << "\tname=" << it.key() << '\n';
    }
    for (auto it = reasonCounts.constBegin();
         it != reasonCounts.constEnd(); ++it)
    {
        *output << "REASON\tchest_occurrences=" << it.value()
                << "\tname=" << it.key() << '\n';
    }
    for (auto it = featureReasonCounts.constBegin();
         it != featureReasonCounts.constEnd(); ++it)
    {
        *output << "PAIR\tchest_occurrences=" << it.value()
                << "\tname=" << it.key() << '\n';
    }
    return true;
}

bool findUnresolved(
    QTextStream *output, QTextStream *errors, int count)
{
    int found = 0;
    for (int index = 0; index < count; index++)
    {
        const uint64_t seed =
            UINT64_C(0x9e3779b97f4a7c15) *
            uint64_t(index + 1);
        const int regionX = (index % 31) - 15;
        const int regionZ = ((index * 11) % 31) - 15;
        Pos start;
        if (!getStructurePos(
                Village, MC_1_16_1, seed,
                regionX, regionZ, &start))
        {
            continue;
        }
        const int chunkX = blockChunk(start.x);
        const int chunkZ = blockChunk(start.z);
        Generator generator;
        setupGenerator(&generator, MC_1_16_1, 0);
        applySeed(&generator, DIM_OVERWORLD, seed);
        const int biome = isViableStructurePos(
            Village, &generator, start.x, start.z, 0);
        if (!biome)
            continue;

        VillageLayout16 layout;
        QString error;
        if (!generateVillageLayout16(
                &layout, seed, chunkX, chunkZ,
                biome, &error))
        {
            *errors << error << '\n';
            return false;
        }
        QVector<VillageLootChestSeed16> chests;
        if (!assignVillageLootSeedsSingleStart16(
                &chests, layout, seed, false, &error))
        {
            *errors << error << '\n';
            return false;
        }
        bool printedStart = false;
        for (const VillageLootChestSeed16& chest : chests)
        {
            if (chest.isExact())
                continue;
            if (!printedStart)
            {
                *output << "START\tseed=" << qint64(seed)
                        << "\tchunk=" << chunkX << ',' << chunkZ
                        << "\tbiome=" << biome << '\n';
                printedStart = true;
                found++;
            }
            *output << "CHEST\tpos="
                    << chest.container.pos.x << ','
                    << chest.container.pos.y << ','
                    << chest.container.pos.z << '\n';
            if (chest.unresolvedFeatureIndex >= 0 &&
                chest.unresolvedFeatureIndex < layout.pieces.size())
            {
                const VillagePiece16& failed = layout.pieces[
                    chest.unresolvedFeatureIndex];
                *output << "FAILED\tindex="
                        << chest.unresolvedFeatureIndex
                        << "\tpos=" << failed.pos.x << ','
                        << failed.pos.y << ',' << failed.pos.z
                        << "\treason="
                        << villageLootUnresolvedReasonName16(
                               chest.unresolvedReason)
                        << "\tfeature=" << failed.feature << '\n';
            }
            for (const VillagePiece16& piece : layout.pieces)
            {
                if (featureBeforeContainerInChunk(
                        layout, chest.container, piece))
                {
                    *output << "BEFORE\tpos="
                            << piece.pos.x << ','
                            << piece.pos.y << ','
                            << piece.pos.z
                            << "\tfeature=" << piece.feature
                            << '\n';
                }
            }
        }
        if (found >= 5)
            break;
    }
    *output << "FOUND\tstarts=" << found << '\n';
    return true;
}

bool printCase(
    QTextStream *output, QTextStream *errors,
    uint64_t seed, int chunkX, int chunkZ, int biome)
{
    VillageLayout16 layout;
    QString error;
    if (!generateVillageLayout16(
            &layout, seed, chunkX, chunkZ,
            biome, &error))
    {
        *errors << error << '\n';
        return false;
    }
    QVector<VillageLootChestSeed16> chests;
    if (!assignVillageLootSeedsSingleStart16(
            &chests, layout, seed, false, &error))
    {
        *errors << error << '\n';
        return false;
    }
    for (const VillageLootChestSeed16& chest : chests)
    {
        *output << "CHEST\tpos="
                << chest.container.pos.x << ','
                << chest.container.pos.y << ','
                << chest.container.pos.z
                << "\tloot_seed="
                << qint64(chest.lootTableSeed)
                << "\tquality=" << chest.quality;
        if (!chest.isExact())
        {
            *output << "\tfailed_feature="
                    << chest.unresolvedFeatureIndex
                    << "\treason="
                    << villageLootUnresolvedReasonName16(
                           chest.unresolvedReason);
        }
        *output << '\n';
    }
    for (int pieceIndex = 0;
         pieceIndex < layout.pieces.size(); pieceIndex++)
    {
        const VillagePiece16& piece = layout.pieces[pieceIndex];
        if (piece.elementType != VillagePiece16::FEATURE)
            continue;
        const auto surface = layout.featureSurfaceHeights.constFind(
            horizontalKey(piece.pos.x, piece.pos.z));
        *output << "FEATURE\tindex=" << pieceIndex
                << "\tpos=" << piece.pos.x << ',' << piece.pos.y
                << ',' << piece.pos.z << "\tsurface="
                << (surface == layout.featureSurfaceHeights.constEnd()
                    ? -1 : *surface)
                << "\tname=" << piece.feature << '\n';
        for (auto blocks = layout.placedBlocks.constBegin();
             blocks != layout.placedBlocks.constEnd(); ++blocks)
        {
            for (const VillagePlacedBlock16& block : *blocks)
            {
                if (block.pieceIndex < pieceIndex &&
                    block.pos.x == piece.pos.x &&
                    block.pos.z == piece.pos.z)
                {
                    *output << "COLUMN_BLOCK\tfeature=" << pieceIndex
                            << "\tpiece=" << block.pieceIndex
                            << "\ty=" << block.pos.y
                            << "\tkind=" << block.kind
                            << "\tknown=" << block.stateKnown << '\n';
                }
                if (block.pieceIndex < pieceIndex &&
                    !block.stateKnown &&
                    qAbs(block.pos.x - piece.pos.x) <= 4 &&
                    qAbs(block.pos.z - piece.pos.z) <= 4 &&
                    block.pos.y >= piece.pos.y - 2 &&
                    block.pos.y <= piece.pos.y + 14)
                {
                    *output << "UNKNOWN_BLOCK\tfeature=" << pieceIndex
                            << "\tpiece=" << block.pieceIndex
                            << "\tpos=" << block.pos.x << ','
                            << block.pos.y << ',' << block.pos.z
                            << "\tkind=" << block.kind
                            << "\tblock=" << block.block << '\n';
                }
            }
        }
    }
    return true;
}

}

int main(int argc, char **argv)
{
#ifdef Q_OS_WIN
    SetErrorMode(
        SEM_FAILCRITICALERRORS |
        SEM_NOGPFAULTERRORBOX |
        SEM_NOOPENFILEERRORBOX);
#endif
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream errors(stderr);

    if (argc == 3 &&
        QString::fromLocal8Bit(argv[1]) ==
            QLatin1String("--scan-unresolved"))
    {
        bool ok;
        const int count = QString::fromLocal8Bit(
            argv[2]).toInt(&ok);
        if (!ok || count <= 0)
        {
            errors << "--scan-unresolved requires a positive count.\n";
            return 1;
        }
        return scanUnresolved(&output, &errors, count) ? 0 : 5;
    }
    if (argc == 3 &&
        QString::fromLocal8Bit(argv[1]) ==
            QLatin1String("--find-unresolved"))
    {
        bool ok;
        const int count = QString::fromLocal8Bit(
            argv[2]).toInt(&ok);
        if (!ok || count <= 0)
        {
            errors << "--find-unresolved requires a positive count.\n";
            return 1;
        }
        return findUnresolved(&output, &errors, count) ? 0 : 6;
    }
    if (argc == 6 &&
        QString::fromLocal8Bit(argv[1]) ==
            QLatin1String("--case"))
    {
        bool seedOk, xOk, zOk, biomeOk;
        const uint64_t seed = uint64_t(
            QString::fromLocal8Bit(argv[2]).toLongLong(
                &seedOk));
        const int chunkX = QString::fromLocal8Bit(
            argv[3]).toInt(&xOk);
        const int chunkZ = QString::fromLocal8Bit(
            argv[4]).toInt(&zOk);
        const int biome = QString::fromLocal8Bit(
            argv[5]).toInt(&biomeOk);
        if (!seedOk || !xOk || !zOk || !biomeOk)
        {
            errors << "--case has an invalid argument.\n";
            return 1;
        }
        return printCase(
            &output, &errors, seed, chunkX, chunkZ,
            biome) ? 0 : 7;
    }
    if (argc != 1)
    {
        errors
            << "Usage: village_loot_seed_probe "
               "[--scan-unresolved count | "
               "--find-unresolved count | "
               "--case seed chunkX chunkZ biomeId]\n";
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
    if (!testFlatFeatureSeed(
            &errors,
            QStringLiteral(
                "Feature.TREE.configured("
                "BiomeDefaultFeatures.NORMAL_TREE_CONFIG)"),
            VillageLayout16::PLAINS,
            INT64_C(3218086560167685275)) ||
        !testFlatFeatureSeed(
            &errors,
            QStringLiteral(
                "Feature.TREE.configured("
                "BiomeDefaultFeatures.PINE_TREE_CONFIG)"),
            VillageLayout16::TAIGA,
            INT64_C(5845126709713115557)) ||
        !testFlatFeatureSeed(
            &errors,
            QStringLiteral(
                "Feature.TREE.configured("
                "BiomeDefaultFeatures.SPRUCE_TREE_CONFIG)"),
            VillageLayout16::TAIGA,
            INT64_C(5845126709713115557)) ||
        !testFlatFeatureSeed(
            &errors,
            QStringLiteral(
                "Feature.TREE.configured("
                "BiomeDefaultFeatures.ACACIA_TREE_CONFIG)"),
            VillageLayout16::SAVANNA,
            INT64_C(-890768451605069389)) ||
        !testFlatFeatureSeed(
            &errors,
            QStringLiteral(
                "Feature.BLOCK_PILE.configured("
                "BiomeDefaultFeatures.HAY_PILE_CONFIG)"),
            VillageLayout16::PLAINS,
            INT64_C(-4948000324792642980)) ||
        !testFlatFeatureSeed(
            &errors,
            QStringLiteral(
                "Feature.BLOCK_PILE.configured("
                "BiomeDefaultFeatures.PUMPKIN_PILE_CONFIG)"),
            VillageLayout16::TAIGA,
            INT64_C(5195020317111493510)) ||
        !testFlatFeatureSeed(
            &errors,
            QStringLiteral(
                "Feature.RANDOM_PATCH.configured("
                "BiomeDefaultFeatures.CACTUS_CONFIG)"),
            VillageLayout16::DESERT,
            INT64_C(-7118312834127643465)))
    {
        return 5;
    }
    output << "SELF_TEST\tok\n";
    return 0;
}
