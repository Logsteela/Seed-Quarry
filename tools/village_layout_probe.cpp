#include "src/villagestructure.h"

#include "cubiomes/generator.h"
#include "cubiomes/loot.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <map>

#ifdef _WIN32
#include <windows.h>
#endif

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

int flatHeight(void *, int, int)
{
    return 64;
}

struct ApproxHeightContext
{
    enum {
        TILE_RADIUS = 24,
        TILE_SIZE = TILE_RADIUS * 2 + 2,
    };

    Generator generator = {};
    SurfaceNoise surfaceNoise = {};
    QVector<float> heights;
    int tileX = 0;
    int tileZ = 0;

    void initialize(uint64_t seed)
    {
        setupGenerator(&generator, MC_1_16_1, 0);
        applySeed(&generator, DIM_OVERWORLD, seed);
        initSurfaceNoise(&surfaceNoise, DIM_OVERWORLD, seed);
    }

    bool ensureTile(int cellX, int cellZ)
    {
        if (!heights.isEmpty())
            return true;
        tileX = cellX - TILE_RADIUS;
        tileZ = cellZ - TILE_RADIUS;
        heights.resize(TILE_SIZE * TILE_SIZE);
        if (mapApproxHeight(
                heights.data(), nullptr,
                &generator, &surfaceNoise,
                tileX, tileZ, TILE_SIZE, TILE_SIZE) != 0)
        {
            heights.clear();
            return false;
        }
        return true;
    }

    int get(int blockX, int blockZ)
    {
        const int cellX = floordiv(blockX, 4);
        const int cellZ = floordiv(blockZ, 4);
        if (!ensureTile(cellX, cellZ))
            return -1;

        float local[4];
        const int x = cellX - tileX;
        const int z = cellZ - tileZ;
        if (x >= 0 && x + 1 < TILE_SIZE &&
            z >= 0 && z + 1 < TILE_SIZE)
        {
            local[0] = heights[z * TILE_SIZE + x];
            local[1] = heights[(z + 1) * TILE_SIZE + x];
            local[2] = heights[z * TILE_SIZE + x + 1];
            local[3] = heights[(z + 1) * TILE_SIZE + x + 1];
        }
        else if (mapApproxHeight(
                     local, nullptr,
                     &generator, &surfaceNoise,
                     cellX, cellZ, 2, 2) != 0)
        {
            return -1;
        }

        int offsetX = blockX - cellX * 4;
        int offsetZ = blockZ - cellZ * 4;
        const double fx = offsetX / 4.0;
        const double fz = offsetZ / 4.0;
        const double north =
            local[0] + (local[2] - local[0]) * fx;
        const double south =
            local[1] + (local[3] - local[1]) * fx;
        const double approximate =
            north + (south - north) * fz;
        return int(std::floor(approximate)) + 1;
    }
};

struct StatelessHeightContext
{
    Generator generator = {};
    SurfaceNoise surfaceNoise = {};

    void initialize(uint64_t seed)
    {
        setupGenerator(&generator, MC_1_16_1, 0);
        applySeed(&generator, DIM_OVERWORLD, seed);
        initSurfaceNoise(&surfaceNoise, DIM_OVERWORLD, seed);
    }
};

int statelessHeight(void *context, int blockX, int blockZ)
{
    StatelessHeightContext *height =
        static_cast<StatelessHeightContext *>(context);
    return getFirstFreeHeight116(
        &height->generator, &height->surfaceNoise,
        blockX, blockZ);
}

int approximateHeight(void *context, int blockX, int blockZ)
{
    return static_cast<ApproxHeightContext *>(context)->get(
        blockX, blockZ);
}

bool sameSearchLayout(
    const VillageLayout16& exact,
    const VillageLayout16& approximate)
{
    if (exact.pieces.size() != approximate.pieces.size() ||
        exact.containers.size() != approximate.containers.size())
    {
        return false;
    }
    for (int i = 0; i < exact.pieces.size(); i++)
    {
        const VillagePiece16& a = exact.pieces[i];
        const VillagePiece16& b = approximate.pieces[i];
        if (a.name != b.name ||
            a.feature != b.feature ||
            a.rotation != b.rotation ||
            a.pos.x != b.pos.x ||
            a.pos.z != b.pos.z)
        {
            return false;
        }
    }
    for (int i = 0; i < exact.containers.size(); i++)
    {
        const VillageContainer16& a = exact.containers[i];
        const VillageContainer16& b = approximate.containers[i];
        if (a.table != b.table ||
            a.pieceIndex != b.pieceIndex ||
            a.placementIndex != b.placementIndex ||
            a.pos.x != b.pos.x ||
            a.pos.z != b.pos.z)
        {
            return false;
        }
    }
    return true;
}

bool sameExactLayout(
    const VillageLayout16& first,
    const VillageLayout16& second)
{
    if (first.villageType != second.villageType ||
        first.biome != second.biome ||
        first.rotation != second.rotation ||
        first.startPool != second.startPool ||
        first.pieces.size() != second.pieces.size() ||
        first.containers.size() != second.containers.size())
    {
        return false;
    }
    for (int i = 0; i < first.pieces.size(); i++)
    {
        const VillagePiece16& a = first.pieces[i];
        const VillagePiece16& b = second.pieces[i];
        if (a.name != b.name ||
            a.feature != b.feature ||
            a.pos.x != b.pos.x ||
            a.pos.y != b.pos.y ||
            a.pos.z != b.pos.z ||
            a.bb0.x != b.bb0.x ||
            a.bb0.y != b.bb0.y ||
            a.bb0.z != b.bb0.z ||
            a.bb1.x != b.bb1.x ||
            a.bb1.y != b.bb1.y ||
            a.bb1.z != b.bb1.z ||
            a.rotation != b.rotation ||
            a.depth != b.depth ||
            a.groundLevelDelta != b.groundLevelDelta ||
            a.elementType != b.elementType ||
            a.terrainMatching != b.terrainMatching)
        {
            return false;
        }
    }
    for (int i = 0; i < first.containers.size(); i++)
    {
        const VillageContainer16& a = first.containers[i];
        const VillageContainer16& b = second.containers[i];
        if (a.pos.x != b.pos.x ||
            a.pos.y != b.pos.y ||
            a.pos.z != b.pos.z ||
            a.block != b.block ||
            a.lootTable != b.lootTable ||
            a.table != b.table ||
            a.pieceIndex != b.pieceIndex ||
            a.placementIndex != b.placementIndex ||
            a.piece != b.piece)
        {
            return false;
        }
    }
    return true;
}

bool verifyCachedHeightCase(
    QTextStream *errors, uint64_t seed,
    int chunkX, int chunkZ, int biome)
{
    QString error;
    VillageLayout16 cached;
    if (!generateVillageLayout16(
            &cached, seed, chunkX, chunkZ, biome, &error))
    {
        *errors << error << '\n';
        return false;
    }
    StatelessHeightContext stateless;
    stateless.initialize(seed);
    VillageLayout16 reference;
    if (!generateVillageLayout16WithHeights(
            &reference, seed, chunkX, chunkZ, biome,
            statelessHeight, &stateless, &error))
    {
        *errors << error << '\n';
        return false;
    }
    if (!sameExactLayout(cached, reference))
    {
        *errors << "Cached height layout differs from stateless layout.\n";
        return false;
    }
    return true;
}

bool sameContainerXZ(
    const VillageLayout16& exact,
    const VillageLayout16& approximate)
{
    if (exact.containers.size() != approximate.containers.size())
        return false;
    for (int i = 0; i < exact.containers.size(); i++)
    {
        const VillageContainer16& a = exact.containers[i];
        const VillageContainer16& b = approximate.containers[i];
        if (a.table != b.table ||
            a.pos.x != b.pos.x ||
            a.pos.z != b.pos.z)
        {
            return false;
        }
    }
    return true;
}

bool sameLootTableCounts(
    const VillageLayout16& exact,
    const VillageLayout16& approximate)
{
    std::map<int, int> exactCounts;
    std::map<int, int> approximateCounts;
    for (const VillageContainer16& container : exact.containers)
    {
        if (container.hasLootTable())
            exactCounts[container.table]++;
    }
    for (const VillageContainer16& container :
         approximate.containers)
    {
        if (container.hasLootTable())
            approximateCounts[container.table]++;
    }
    return exactCounts == approximateCounts;
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
#ifdef _WIN32
    SetErrorMode(
        SEM_FAILCRITICALERRORS |
        SEM_NOGPFAULTERRORBOX |
        SEM_NOOPENFILEERRORBOX);
#endif
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream errors(stderr);

    bool summaryOnly = false;
    bool selfTest = false;
    int benchmarkCount = 0;
    bool benchmarkFlat = false;
    int compareApproxCount = 0;
    bool compareFlat = false;
    int argument = 1;
    while (argument < argc)
    {
        const QString option =
            QString::fromLocal8Bit(argv[argument]);
        if (option == QLatin1String("--summary"))
            summaryOnly = true;
        else if (option == QLatin1String("--self-test"))
            selfTest = true;
        else if (option == QLatin1String("--benchmark") ||
                 option == QLatin1String("--benchmark-flat"))
        {
            benchmarkFlat =
                option == QLatin1String("--benchmark-flat");
            if (++argument >= argc)
            {
                errors << "--benchmark requires a positive count.\n";
                return 1;
            }
            bool countOk;
            benchmarkCount = QString::fromLocal8Bit(
                argv[argument]).toInt(&countOk);
            if (!countOk || benchmarkCount <= 0)
            {
                errors << "--benchmark requires a positive count.\n";
                return 1;
            }
        }
        else if (option == QLatin1String("--compare-approx") ||
                 option == QLatin1String("--compare-flat"))
        {
            compareFlat =
                option == QLatin1String("--compare-flat");
            if (++argument >= argc)
            {
                errors << "--compare-approx requires a positive count.\n";
                return 1;
            }
            bool countOk;
            compareApproxCount = QString::fromLocal8Bit(
                argv[argument]).toInt(&countOk);
            if (!countOk || compareApproxCount <= 0)
            {
                errors << "--compare-approx requires a positive count.\n";
                return 1;
            }
        }
        else
            break;
        argument++;
    }

    output << "DATA\tpath=" << villageStructureData16Path() << '\n';
    if (compareApproxCount > 0)
    {
        if (argc != argument)
        {
            errors << "--compare-approx does not accept a seed.\n";
            return 1;
        }
        static const int biomes[] = {
            plains, desert, savanna, snowy_tundra, taiga,
        };
        int same = 0;
        int sameContainers = 0;
        int sameLootCounts = 0;
        qint64 exactNs = 0;
        qint64 approximateNs = 0;
        for (int index = 0; index < compareApproxCount; index++)
        {
            const uint64_t seed =
                UINT64_C(0x9e3779b97f4a7c15) *
                uint64_t(index + 1);
            const int chunkX = (index % 127) - 63;
            const int chunkZ = ((index * 37) % 127) - 63;
            const int biome = biomes[index % 5];
            QString error;
            VillageLayout16 exact;
            QElapsedTimer exactTimer;
            exactTimer.start();
            if (!generateVillageLayout16(
                    &exact, seed, chunkX, chunkZ,
                    biome, &error))
            {
                errors << error << '\n';
                return 2;
            }
            exactNs += exactTimer.nsecsElapsed();

            VillageLayout16 approximate;
            QElapsedTimer approximateTimer;
            approximateTimer.start();
            ApproxHeightContext context;
            if (!compareFlat)
                context.initialize(seed);
            const bool approximateOk =
                generateVillageLayout16WithHeights(
                    &approximate, seed, chunkX, chunkZ,
                    biome,
                    compareFlat
                        ? flatHeight
                        : approximateHeight,
                    compareFlat ? nullptr : &context,
                    &error);
            if (!approximateOk)
            {
                errors << error << '\n';
                return 3;
            }
            approximateNs +=
                approximateTimer.nsecsElapsed();
            if (sameSearchLayout(exact, approximate))
                same++;
            if (sameContainerXZ(exact, approximate))
                sameContainers++;
            if (sameLootTableCounts(exact, approximate))
                sameLootCounts++;
        }
        output
            << "APPROX_COMPARE"
            << "\tmode=" << (compareFlat ? "flat" : "map")
            << "\tcount=" << compareApproxCount
            << "\tsame=" << same
            << "\tsame_percent="
            << QString::number(
                   same * 100.0 / compareApproxCount,
                   'f', 2)
            << "\tsame_containers_percent="
            << QString::number(
                   sameContainers * 100.0 /
                       compareApproxCount,
                   'f', 2)
            << "\tsame_loot_table_counts_percent="
            << QString::number(
                   sameLootCounts * 100.0 /
                       compareApproxCount,
                   'f', 2)
            << "\texact_per_layout_us="
            << QString::number(
                   exactNs / 1000.0 / compareApproxCount,
                   'f', 3)
            << "\tapprox_per_layout_us="
            << QString::number(
                   approximateNs / 1000.0 /
                       compareApproxCount,
                   'f', 3)
            << '\n';
        return 0;
    }
    if (benchmarkCount > 0)
    {
        if (argc != argument)
        {
            errors << "--benchmark does not accept a seed.\n";
            return 1;
        }

        // Warm the manifest and static lookup tables before timing. The
        // benchmark intentionally retains the normal per-layout Cubiomes
        // generator/surface-noise initialization used by the GUI search.
        VillageLayout16 warm;
        QString error;
        const bool warmOk = benchmarkFlat
            ? generateVillageLayout16WithHeights(
                  &warm, 0, -25, 21, taiga,
                  flatHeight, nullptr, &error)
            : generateVillageLayout16(
                  &warm, 0, -25, 21, taiga, &error);
        if (!warmOk)
        {
            errors << error << '\n';
            return 2;
        }

        static const int biomes[] = {
            plains, desert, savanna, snowy_tundra, taiga,
        };
        quint64 pieces = 0;
        quint64 containers = 0;
        QElapsedTimer timer;
        timer.start();
        for (int index = 0; index < benchmarkCount; index++)
        {
            VillageLayout16 layout;
            const uint64_t seed =
                UINT64_C(0x9e3779b97f4a7c15) *
                uint64_t(index + 1);
            const int chunkX = (index % 127) - 63;
            const int chunkZ = ((index * 37) % 127) - 63;
            const bool generated = benchmarkFlat
                ? generateVillageLayout16WithHeights(
                      &layout, seed, chunkX, chunkZ,
                      biomes[index % 5],
                      flatHeight, nullptr, &error)
                : generateVillageLayout16(
                      &layout, seed, chunkX, chunkZ,
                      biomes[index % 5], &error);
            if (!generated)
            {
                errors << error << '\n';
                return 3;
            }
            pieces += quint64(layout.pieces.size());
            containers += quint64(layout.containers.size());
        }
        const qint64 elapsedNs = timer.nsecsElapsed();
        const double elapsedMs =
            double(elapsedNs) / 1000000.0;
        output
            << "BENCHMARK"
            << "\tmode=" << (benchmarkFlat ? "flat" : "exact")
            << "\tcount=" << benchmarkCount
            << "\ttotal_ms="
            << QString::number(elapsedMs, 'f', 3)
            << "\tper_layout_us="
            << QString::number(
                   elapsedMs * 1000.0 /
                       double(benchmarkCount),
                   'f', 3)
            << "\tpieces=" << pieces
            << "\tcontainers=" << containers
            << '\n';
        return 0;
    }
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
        if (!verifyCachedHeightCase(
                &errors, 0, -25, 21, taiga) ||
            !verifyCachedHeightCase(
                &errors, UINT64_C(8040347553),
                9, 20, savanna))
        {
            return 4;
        }
        output << "SELF_TEST\tok\n";
        return 0;
    }

    if (argc - argument != 4)
    {
        errors
            << "Usage: village_layout_probe [--summary] "
               "[--self-test | --benchmark count] "
               "[--benchmark-flat count] "
               "[--compare-approx count | --compare-flat count] "
               "seed chunkX chunkZ "
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
