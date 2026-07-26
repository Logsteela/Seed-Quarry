#include "src/villagestructure.h"

#include "cubiomes/loot.h"

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>

namespace {

QString rotationName(int rotation)
{
    switch (rotation)
    {
    case 1: return QStringLiteral("CLOCKWISE_90");
    case 2: return QStringLiteral("CLOCKWISE_180");
    case 3: return QStringLiteral("COUNTERCLOCKWISE_90");
    default: return QStringLiteral("NONE");
    }
}

QString biomeName(int biome)
{
    switch (biome)
    {
    case plains: return QStringLiteral("plains");
    case desert: return QStringLiteral("desert");
    case savanna: return QStringLiteral("savanna");
    case snowy_tundra: return QStringLiteral("snowy");
    case taiga: return QStringLiteral("taiga");
    default: return QString::number(biome);
    }
}

bool parseBiome(const QString& value, int *biome)
{
    const QString lower = value.toLower();
    if (lower == QLatin1String("plains"))
        *biome = plains;
    else if (lower == QLatin1String("desert"))
        *biome = desert;
    else if (lower == QLatin1String("savanna"))
        *biome = savanna;
    else if (lower == QLatin1String("snowy") ||
             lower == QLatin1String("snowy_tundra"))
        *biome = snowy_tundra;
    else if (lower == QLatin1String("taiga"))
        *biome = taiga;
    else
    {
        bool ok;
        const int parsed = value.toInt(&ok);
        if (!ok)
            return false;
        *biome = parsed;
    }
    return true;
}

void printLayout(
    QTextStream *output, uint64_t seed,
    int chunkX, int chunkZ, const VillageLayout16& layout,
    bool summaryOnly)
{
    Pos3 bb0 = {0, 0, 0};
    Pos3 bb1 = {0, 0, 0};
    if (!layout.pieces.isEmpty())
    {
        bb0 = layout.pieces.first().bb0;
        bb1 = layout.pieces.first().bb1;
        for (const VillagePiece16& piece : layout.pieces)
        {
            bb0.x = std::min(bb0.x, piece.bb0.x);
            bb0.y = std::min(bb0.y, piece.bb0.y);
            bb0.z = std::min(bb0.z, piece.bb0.z);
            bb1.x = std::max(bb1.x, piece.bb1.x);
            bb1.y = std::max(bb1.y, piece.bb1.y);
            bb1.z = std::max(bb1.z, piece.bb1.z);
        }
        // BeardedStructureStart expands the completed piece union by 12
        // blocks in every direction.
        bb0.x -= 12;
        bb0.y -= 12;
        bb0.z -= 12;
        bb1.x += 12;
        bb1.y += 12;
        bb1.z += 12;
    }

    *output
        << "START"
        << "\tseed=" << qint64(seed)
        << "\tchunk=" << chunkX << ',' << chunkZ
        << "\tbiome=" << biomeName(layout.biome)
        << "\tpool=minecraft:" << layout.startPool
        << "\trotation=" << rotationName(layout.rotation)
        << "\tpieces=" << layout.pieceCount
        << "\tcontainers=" << layout.containers.size()
        << "\tbbox=" << bb0.x << ',' << bb0.y << ',' << bb0.z
        << ':' << bb1.x << ',' << bb1.y << ',' << bb1.z
        << '\n';
    if (summaryOnly)
        return;

    for (int index = 0; index < layout.pieces.size(); index++)
    {
        const VillagePiece16& piece = layout.pieces[index];
        const QString type =
            piece.elementType == VillagePiece16::FEATURE
            ? QStringLiteral("minecraft:feature_pool_element")
            : QStringLiteral("minecraft:legacy_single_pool_element");
        const QString descriptor =
            piece.elementType == VillagePiece16::FEATURE
            ? QStringLiteral("feature:") +
                piece.name.mid(8, piece.name.size() - 9)
            : QStringLiteral("minecraft:") + piece.name;
        *output
            << "PIECE"
            << "\tindex=" << index
            << "\ttype=" << type
            << "\ttemplate=" << descriptor
            << "\trotation=" << rotationName(piece.rotation)
            << "\tpos=" << piece.pos.x << ',' << piece.pos.y
            << ',' << piece.pos.z
            << "\tbbox=" << piece.bb0.x << ',' << piece.bb0.y
            << ',' << piece.bb0.z << ':' << piece.bb1.x
            << ',' << piece.bb1.y << ',' << piece.bb1.z
            << "\tdepth=" << piece.depth
            << "\tprojection="
            << (piece.terrainMatching
                ? "terrain_matching" : "rigid")
            << "\tground_delta=" << piece.groundLevelDelta;
        if (!piece.feature.isEmpty())
            *output << "\tfeature=" << piece.feature;
        *output << '\n';
    }

    for (int index = 0; index < layout.containers.size(); index++)
    {
        const VillageContainer16& container =
            layout.containers[index];
        *output
            << "CONTAINER"
            << "\tindex=" << index
            << "\tpiece_index=" << container.pieceIndex
            << "\tplacement_index=" << container.placementIndex
            << "\tpos=" << container.pos.x << ','
            << container.pos.y << ',' << container.pos.z
            << "\tblock=minecraft:" << container.block
            << "\tloot_table=";
        if (container.hasLootTable())
            *output << "minecraft:" << container.lootTable;
        else
            *output << '-';
        *output
            << "\ttable_id=" << container.table
            << "\tpiece=minecraft:" << container.piece
            << '\n';
    }
}

bool runCase(
    QTextStream *output, QTextStream *errors,
    uint64_t seed, int chunkX, int chunkZ, int biome,
    int expectedPieces, bool summaryOnly)
{
    QString error;
    VillageLayout16 layout;
    if (!generateVillageLayout16(
            &layout, seed, chunkX, chunkZ, biome, &error))
    {
        *errors << error << '\n';
        return false;
    }
    printLayout(
        output, seed, chunkX, chunkZ, layout, summaryOnly);
    if (expectedPieces >= 0 &&
        layout.pieceCount != expectedPieces)
    {
        *errors
            << "Expected " << expectedPieces
            << " pieces, received " << layout.pieceCount
            << ".\n";
        return false;
    }
    return true;
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream errors(stderr);

    bool summaryOnly = false;
    bool selfTest = false;
    int argument = 1;
    while (argument < argc)
    {
        const QString option =
            QString::fromLocal8Bit(argv[argument]);
        if (option == QLatin1String("--summary"))
            summaryOnly = true;
        else if (option == QLatin1String("--self-test"))
            selfTest = true;
        else
            break;
        argument++;
    }

    output << "DATA\tpath=" << villageStructureData16Path() << '\n';
    if (selfTest)
    {
        if (argc != argument)
        {
            errors << "--self-test does not accept a seed.\n";
            return 1;
        }
        if (!runCase(
                &output, &errors, 0, -25, 21, taiga,
                62, true))
        {
            return 2;
        }
        if (!runCase(
                &output, &errors, UINT64_C(8040347553),
                9, 20, savanna, 140, true))
        {
            return 3;
        }
        output << "SELF_TEST\tok\n";
        return 0;
    }

    if (argc - argument != 4)
    {
        errors
            << "Usage: village_layout_probe [--summary] "
               "[--self-test] seed chunkX chunkZ "
               "(plains|desert|savanna|snowy|taiga)\n";
        return 1;
    }

    bool seedOk, xOk, zOk;
    const uint64_t seed = uint64_t(
        QString::fromLocal8Bit(argv[argument]).toLongLong(
            &seedOk, 10));
    const int chunkX = QString::fromLocal8Bit(
        argv[argument + 1]).toInt(&xOk);
    const int chunkZ = QString::fromLocal8Bit(
        argv[argument + 2]).toInt(&zOk);
    int biome;
    const bool biomeOk = parseBiome(
        QString::fromLocal8Bit(argv[argument + 3]), &biome);
    if (!seedOk || !xOk || !zOk || !biomeOk)
    {
        errors << "Invalid seed, chunk, or biome.\n";
        return 1;
    }

    return runCase(
        &output, &errors, seed, chunkX, chunkZ,
        biome, -1, summaryOnly) ? 0 : 2;
}
