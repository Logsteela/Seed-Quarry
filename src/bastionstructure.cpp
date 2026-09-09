#include "bastionstructure.h"

#include "cubiomes/loot.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include <algorithm>
#include <deque>
#include <memory>
#include <utility>

namespace {

enum Direction16
{
    DIR_DOWN,
    DIR_UP,
    DIR_NORTH,
    DIR_SOUTH,
    DIR_WEST,
    DIR_EAST,
    DIR_INVALID,
};

struct Point3
{
    int x = 0;
    int y = 0;
    int z = 0;
};

struct Box3
{
    int x0 = 0;
    int y0 = 0;
    int z0 = 0;
    int x1 = -1;
    int y1 = -1;
    int z1 = -1;

    bool contains(const Point3& point) const
    {
        return point.x >= x0 && point.x <= x1 &&
            point.y >= y0 && point.y <= y1 &&
            point.z >= z0 && point.z <= z1;
    }
};

struct Jigsaw16
{
    Point3 pos;
    Direction16 front = DIR_INVALID;
    Direction16 top = DIR_INVALID;
    QString pool;
    QString target;
    QString name;
    bool rollable = false;
    int placementIndex = 0;
};

struct Container16
{
    Point3 pos;
    int table = -1;
    int placementIndex = 0;
};

struct Template16
{
    QString name;
    Point3 size;
    QVector<Jigsaw16> jigsaws;
    QVector<Container16> containers;
};

struct Pool16
{
    QString name;
    QString fallback;
    QVector<int> templates;
};

struct Piece16
{
    int templateIndex = -1;
    Point3 origin;
    int rotation = 0;
    int depth = 0;
    Box3 box;
};

struct HalfOpenBox
{
    double x0 = 0;
    double y0 = 0;
    double z0 = 0;
    double x1 = 0;
    double y1 = 0;
    double z1 = 0;
};

struct FreeRegion
{
    HalfOpenBox outer;
    QVector<HalfOpenBox> occupied;
};

struct PieceState
{
    int pieceIndex = -1;
    std::shared_ptr<FreeRegion> free;
    int depth = 0;
};

struct PendingChest
{
    BastionLootChest16 chest;
    int pieceIndex = -1;
    int placementIndex = 0;
};

struct BastionData16
{
    bool valid = false;
    QString path;
    QString error;
    QVector<Template16> templates;
    QVector<Pool16> pools;
    QHash<QString, int> templateByName;
    QHash<QString, int> poolByName;
};

QString withoutMinecraftNamespace(QString name)
{
    if (name.startsWith(QLatin1String("minecraft:")))
        name.remove(0, 10);
    return name;
}

QString normalizedTemplateName(QString name)
{
    name = withoutMinecraftNamespace(name);
    if (name.startsWith(QLatin1String("bastion/")))
        name.remove(0, 8);
    return name;
}

Direction16 parseDirection(const QString& name)
{
    if (name == QLatin1String("down"))
        return DIR_DOWN;
    if (name == QLatin1String("up"))
        return DIR_UP;
    if (name == QLatin1String("north"))
        return DIR_NORTH;
    if (name == QLatin1String("south"))
        return DIR_SOUTH;
    if (name == QLatin1String("west"))
        return DIR_WEST;
    if (name == QLatin1String("east"))
        return DIR_EAST;
    return DIR_INVALID;
}

Point3 pointFromJson(const QJsonValue& value, bool *ok)
{
    Point3 point;
    QJsonArray array = value.toArray();
    if (array.size() != 3)
    {
        *ok = false;
        return point;
    }
    point.x = array[0].toInt();
    point.y = array[1].toInt();
    point.z = array[2].toInt();
    return point;
}

int tableFromName(QString name)
{
    name = withoutMinecraftNamespace(name);
    if (name == QLatin1String("chests/bastion_bridge"))
        return LOOT_TABLE16_BASTION_BRIDGE;
    if (name == QLatin1String("chests/bastion_hoglin_stable"))
        return LOOT_TABLE16_BASTION_HOGLIN_STABLE;
    if (name == QLatin1String("chests/bastion_other"))
        return LOOT_TABLE16_BASTION_OTHER;
    if (name == QLatin1String("chests/bastion_treasure"))
        return LOOT_TABLE16_BASTION_TREASURE;
    return -1;
}

QStringList manifestCandidates()
{
    QStringList paths;
    const QString configured =
        qEnvironmentVariable("SEED_QUARRY_STRUCTURE_DATA");
    if (!configured.isEmpty())
    {
        QFileInfo info(configured);
        paths << (info.isDir()
            ? QDir(configured).filePath(
                QStringLiteral("jigsaw-1.16.1.json"))
            : configured);
    }

    if (QCoreApplication::instance())
    {
        QDir app(QCoreApplication::applicationDirPath());
        paths << app.filePath(QStringLiteral("jigsaw-1.16.1.json"));
        paths << app.filePath(
            QStringLiteral("structure-data/jigsaw-1.16.1.json"));
        paths << app.filePath(
            QStringLiteral("../../build-structure-data/"
                           "jigsaw-1.16.1.json"));
    }
    paths << QDir::current().filePath(
        QStringLiteral("build-structure-data/jigsaw-1.16.1.json"));
    paths.removeDuplicates();
    return paths;
}

bool runtimeOrderLess(
    int firstPlacement, const Point3& first,
    int secondPlacement, const Point3& second)
{
    // StructureTemplate.buildInfoList orders all NBT-bearing blocks by Y/X/Z.
    // placement_index is retained as a deterministic final tie-breaker.
    if (first.y != second.y)
        return first.y < second.y;
    if (first.x != second.x)
        return first.x < second.x;
    if (first.z != second.z)
        return first.z < second.z;
    return firstPlacement < secondPlacement;
}

BastionData16 loadBastionData16()
{
    BastionData16 data;
    for (const QString& candidate : manifestCandidates())
    {
        QFileInfo info(candidate);
        if (info.isFile())
        {
            data.path = info.absoluteFilePath();
            break;
        }
    }
    if (data.path.isEmpty())
    {
        data.error = QCoreApplication::translate("BastionStructure",
            "The Java 1.16.1 structure data file jigsaw-1.16.1.json was not found. "
            "Run rebuild to deploy it.");
        return data;
    }

    QFile file(data.path);
    if (!file.open(QIODevice::ReadOnly))
    {
        data.error = QCoreApplication::translate("BastionStructure",
            "Could not read the Java 1.16.1 structure data: ") +
            file.errorString();
        return data;
    }
    QJsonParseError parseError;
    QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject())
    {
        data.error = QCoreApplication::translate("BastionStructure",
            "The Java 1.16.1 structure data is invalid: ") +
            parseError.errorString();
        return data;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toInt() != 1 ||
        root.value(QStringLiteral("minecraft_version")).toString() !=
            QLatin1String("1.16.1") ||
        root.value(QStringLiteral("jar_sha1")).toString() !=
            QLatin1String("c9abbe8ee4fa490751ca70635340b7cf00db83ff"))
    {
        data.error = QCoreApplication::translate("BastionStructure",
            "The structure-data format, Minecraft version, or official JAR SHA-1 does not match.");
        return data;
    }

    bool ok = true;
    const QJsonArray structures = root
        .value(QStringLiteral("structures")).toObject()
        .value(QStringLiteral("bastion")).toArray();
    data.templates.reserve(structures.size());
    int containerCount = 0;
    for (const QJsonValue& value : structures)
    {
        const QJsonObject object = value.toObject();
        Template16 structure;
        structure.name = normalizedTemplateName(
            object.value(QStringLiteral("name")).toString());
        structure.size =
            pointFromJson(object.value(QStringLiteral("size")), &ok);
        if (structure.name.isEmpty() ||
            structure.size.x <= 0 || structure.size.y <= 0 ||
            structure.size.z <= 0 ||
            data.templateByName.contains(structure.name))
        {
            ok = false;
            break;
        }

        const QJsonArray jigsaws =
            object.value(QStringLiteral("jigsaws")).toArray();
        structure.jigsaws.reserve(jigsaws.size());
        for (const QJsonValue& jigsawValue : jigsaws)
        {
            const QJsonObject jigsawObject = jigsawValue.toObject();
            Jigsaw16 jigsaw;
            jigsaw.pos = pointFromJson(
                jigsawObject.value(QStringLiteral("pos")), &ok);
            const QString orientation =
                jigsawObject.value(
                    QStringLiteral("orientation")).toString();
            const QStringList directions =
                orientation.split(QLatin1Char('_'));
            if (directions.size() != 2)
            {
                ok = false;
                break;
            }
            jigsaw.front = parseDirection(directions[0]);
            jigsaw.top = parseDirection(directions[1]);
            jigsaw.pool = withoutMinecraftNamespace(
                jigsawObject.value(
                    QStringLiteral("pool")).toString());
            jigsaw.target = withoutMinecraftNamespace(
                jigsawObject.value(
                    QStringLiteral("target")).toString());
            jigsaw.name = withoutMinecraftNamespace(
                jigsawObject.value(
                    QStringLiteral("name")).toString());
            jigsaw.rollable =
                jigsawObject.value(QStringLiteral("joint")).toString() ==
                QLatin1String("rollable");
            jigsaw.placementIndex = jigsawObject.value(
                QStringLiteral("placement_index")).toInt();
            if (jigsaw.front == DIR_INVALID ||
                jigsaw.top == DIR_INVALID ||
                jigsaw.pool.isEmpty())
            {
                ok = false;
                break;
            }
            structure.jigsaws.push_back(jigsaw);
        }
        if (!ok)
            break;
        std::sort(
            structure.jigsaws.begin(), structure.jigsaws.end(),
            [](const Jigsaw16& first, const Jigsaw16& second) {
                return runtimeOrderLess(
                    first.placementIndex, first.pos,
                    second.placementIndex, second.pos);
            });

        const QJsonArray containers =
            object.value(QStringLiteral("containers")).toArray();
        structure.containers.reserve(containers.size());
        for (const QJsonValue& containerValue : containers)
        {
            const QJsonObject containerObject =
                containerValue.toObject();
            Container16 container;
            container.pos = pointFromJson(
                containerObject.value(QStringLiteral("pos")), &ok);
            container.table = tableFromName(
                containerObject.value(
                    QStringLiteral("loot_table")).toString());
            container.placementIndex = containerObject.value(
                QStringLiteral("placement_index")).toInt();
            if (container.table < 0)
            {
                ok = false;
                break;
            }
            structure.containers.push_back(container);
            containerCount++;
        }
        if (!ok)
            break;
        std::sort(
            structure.containers.begin(), structure.containers.end(),
            [](const Container16& first, const Container16& second) {
                return runtimeOrderLess(
                    first.placementIndex, first.pos,
                    second.placementIndex, second.pos);
            });

        data.templateByName.insert(
            structure.name, data.templates.size());
        data.templates.push_back(structure);
    }

    const QJsonArray definitions = root
        .value(QStringLiteral("pools")).toObject()
        .value(QStringLiteral("bastion")).toObject()
        .value(QStringLiteral("definitions")).toArray();
    data.pools.reserve(definitions.size());
    for (const QJsonValue& value : definitions)
    {
        const QJsonObject object = value.toObject();
        Pool16 pool;
        pool.name = withoutMinecraftNamespace(
            object.value(QStringLiteral("name")).toString());
        pool.fallback = withoutMinecraftNamespace(
            object.value(QStringLiteral("fallback")).toString());
        if (pool.name.isEmpty() ||
            data.poolByName.contains(pool.name) ||
            object.value(QStringLiteral("projection")).toString() !=
                QLatin1String("rigid"))
        {
            ok = false;
            break;
        }
        const QJsonArray elements =
            object.value(QStringLiteral("elements")).toArray();
        for (const QJsonValue& elementValue : elements)
        {
            const QJsonObject element = elementValue.toObject();
            const QString templateName = normalizedTemplateName(
                element.value(QStringLiteral("template")).toString());
            const int weight =
                element.value(QStringLiteral("weight")).toInt();
            const auto found = data.templateByName.constFind(templateName);
            if (found == data.templateByName.constEnd() ||
                weight <= 0 || weight > 1000)
            {
                ok = false;
                break;
            }
            for (int occurrence = 0; occurrence < weight; occurrence++)
                pool.templates.push_back(*found);
        }
        if (!ok)
            break;
        data.poolByName.insert(pool.name, data.pools.size());
        data.pools.push_back(pool);
    }

    static const char *startPools[] = {
        "bastion/units/base",
        "bastion/hoglin_stable/origin",
        "bastion/treasure/starters",
        "bastion/bridge/start",
    };
    for (const char *startPool : startPools)
    {
        if (!data.poolByName.contains(QLatin1String(startPool)))
            ok = false;
    }

    if (!ok || data.templates.size() != 167 ||
        data.pools.size() != 63 || containerCount != 37)
    {
        data.error = QCoreApplication::translate("BastionStructure",
            "The structure data does not match the expected Java 1.16.1 contents "
            "(167 templates, 63 pools, and 37 containers).");
        data.templates.clear();
        data.pools.clear();
        data.templateByName.clear();
        data.poolByName.clear();
        return data;
    }

    data.valid = true;
    return data;
}

const BastionData16& bastionData16()
{
    static const BastionData16 data = loadBastionData16();
    return data;
}

Point3 add(const Point3& first, const Point3& second)
{
    return {
        first.x + second.x,
        first.y + second.y,
        first.z + second.z,
    };
}

Point3 subtract(const Point3& first, const Point3& second)
{
    return {
        first.x - second.x,
        first.y - second.y,
        first.z - second.z,
    };
}

Point3 directionStep(Direction16 direction)
{
    switch (direction)
    {
    case DIR_DOWN:  return { 0, -1,  0};
    case DIR_UP:    return { 0,  1,  0};
    case DIR_NORTH: return { 0,  0, -1};
    case DIR_SOUTH: return { 0,  0,  1};
    case DIR_WEST:  return {-1,  0,  0};
    case DIR_EAST:  return { 1,  0,  0};
    default:        return {};
    }
}

Direction16 opposite(Direction16 direction)
{
    switch (direction)
    {
    case DIR_DOWN:  return DIR_UP;
    case DIR_UP:    return DIR_DOWN;
    case DIR_NORTH: return DIR_SOUTH;
    case DIR_SOUTH: return DIR_NORTH;
    case DIR_WEST:  return DIR_EAST;
    case DIR_EAST:  return DIR_WEST;
    default:        return DIR_INVALID;
    }
}

Point3 rotatePoint(const Point3& point, int rotation)
{
    switch (rotation)
    {
    case 1: return {-point.z, point.y,  point.x};
    case 2: return {-point.x, point.y, -point.z};
    case 3: return { point.z, point.y, -point.x};
    default: return point;
    }
}

Direction16 rotateDirection(Direction16 direction, int rotation)
{
    if (direction == DIR_UP || direction == DIR_DOWN)
        return direction;
    for (int step = 0; step < rotation; step++)
    {
        switch (direction)
        {
        case DIR_NORTH: direction = DIR_EAST; break;
        case DIR_EAST:  direction = DIR_SOUTH; break;
        case DIR_SOUTH: direction = DIR_WEST; break;
        case DIR_WEST:  direction = DIR_NORTH; break;
        default: return DIR_INVALID;
        }
    }
    return direction;
}

Box3 templateBox(
    const Template16& structure, const Point3& origin, int rotation)
{
    Box3 box;
    const int sizeX = structure.size.x - 1;
    const int sizeY = structure.size.y - 1;
    const int sizeZ = structure.size.z - 1;
    switch (rotation)
    {
    case 1:
        box = {-sizeZ, 0, 0, 0, sizeY, sizeX};
        break;
    case 2:
        box = {-sizeX, 0, -sizeZ, 0, sizeY, 0};
        break;
    case 3:
        box = {0, 0, -sizeX, sizeZ, sizeY, 0};
        break;
    default:
        box = {0, 0, 0, sizeX, sizeY, sizeZ};
        break;
    }
    box.x0 += origin.x;
    box.x1 += origin.x;
    box.y0 += origin.y;
    box.y1 += origin.y;
    box.z0 += origin.z;
    box.z1 += origin.z;
    return box;
}

HalfOpenBox halfOpen(const Box3& box)
{
    return {
        double(box.x0), double(box.y0), double(box.z0),
        double(box.x1) + 1.0,
        double(box.y1) + 1.0,
        double(box.z1) + 1.0,
    };
}

bool intersects(const HalfOpenBox& first, const HalfOpenBox& second)
{
    return first.x0 < second.x1 && first.x1 > second.x0 &&
        first.y0 < second.y1 && first.y1 > second.y0 &&
        first.z0 < second.z1 && first.z1 > second.z0;
}

bool reserveBox(FreeRegion *region, const Box3& box)
{
    if (!region)
        return false;
    HalfOpenBox candidate = {
        double(box.x0) + 0.25,
        double(box.y0) + 0.25,
        double(box.z0) + 0.25,
        double(box.x1) + 0.75,
        double(box.y1) + 0.75,
        double(box.z1) + 0.75,
    };
    if (candidate.x0 < region->outer.x0 ||
        candidate.y0 < region->outer.y0 ||
        candidate.z0 < region->outer.z0 ||
        candidate.x1 > region->outer.x1 ||
        candidate.y1 > region->outer.y1 ||
        candidate.z1 > region->outer.z1)
    {
        return false;
    }
    for (const HalfOpenBox& occupied : region->occupied)
    {
        if (intersects(candidate, occupied))
            return false;
    }
    region->occupied.push_back(halfOpen(box));
    return true;
}

template <typename T>
void javaShuffle(QVector<T> *values, uint64_t *random)
{
    for (int size = values->size(); size > 1; size--)
    {
        const int selected = nextInt(random, size);
        if (selected != size - 1)
            values->swapItemsAt(selected, size - 1);
    }
}

QVector<Jigsaw16> shuffledJigsaws(
    const Template16& structure, const Piece16& piece,
    uint64_t *random)
{
    QVector<Jigsaw16> result = structure.jigsaws;
    for (Jigsaw16& jigsaw : result)
    {
        jigsaw.pos = add(
            rotatePoint(jigsaw.pos, piece.rotation),
            piece.origin);
        jigsaw.front =
            rotateDirection(jigsaw.front, piece.rotation);
        jigsaw.top =
            rotateDirection(jigsaw.top, piece.rotation);
    }
    javaShuffle(&result, random);
    return result;
}

QVector<int> shuffledPool(
    const BastionData16& data, const QString& name,
    uint64_t *random)
{
    const auto found = data.poolByName.constFind(name);
    if (found == data.poolByName.constEnd())
        return {};
    QVector<int> result = data.pools[*found].templates;
    javaShuffle(&result, random);
    /*
     * Vanilla uses fastutil ObjectArrays.shuffle for pool elements.
     * Unlike Collections.shuffle (used by jigsaws and rotations), its loop
     * includes the final nextInt(1) call.
     */
    if (!result.isEmpty())
        (void) nextInt(random, 1);
    return result;
}

bool canAttach(const Jigsaw16& source, const Jigsaw16& candidate)
{
    return source.front == opposite(candidate.front) &&
        (source.rollable || source.top == candidate.top) &&
        source.target == candidate.name;
}

qint64 chunkKey(int chunkX, int chunkZ)
{
    return qint64(
        (quint64(quint32(chunkX)) << 32) |
        quint64(quint32(chunkZ)));
}

int floorChunk(int coordinate)
{
    return floordiv(coordinate, 16);
}

void assignLootSeeds(
    QVector<PendingChest> *pending, uint64_t worldSeed)
{
    QHash<qint64, QVector<int>> byChunk;
    for (int index = 0; index < pending->size(); index++)
    {
        const Pos3& pos = (*pending)[index].chest.pos;
        byChunk[chunkKey(
            floorChunk(pos.x), floorChunk(pos.z))].push_back(index);
    }

    for (auto group = byChunk.begin(); group != byChunk.end(); ++group)
    {
        const int chunkX = qint32(quint64(group.key()) >> 32);
        const int chunkZ = qint32(quint32(group.key()));
        uint64_t random;
        setSeed(
            &random,
            getPopulationSeed(
                MC_1_16_1, worldSeed,
                chunkX * 16, chunkZ * 16) +
                UINT64_C(40012));

        QVector<int>& indices = group.value();
        std::sort(
            indices.begin(), indices.end(),
            [pending](int first, int second) {
                const PendingChest& a = (*pending)[first];
                const PendingChest& b = (*pending)[second];
                if (a.pieceIndex != b.pieceIndex)
                    return a.pieceIndex < b.pieceIndex;
                return a.placementIndex < b.placementIndex;
            });
        for (int index : indices)
            (*pending)[index].chest.lootTableSeed =
                nextLong(&random);
    }
}

}

bool isBastionStructureData16Available(QString *error)
{
    const BastionData16& data = bastionData16();
    if (error)
        *error = data.error;
    return data.valid;
}

QString bastionStructureData16Path()
{
    return bastionData16().path;
}

static bool generateBastionLayout16Internal(
    BastionLayout16 *out, uint64_t worldSeed,
    int chunkX, int chunkZ, bool includePieces, QString *error)
{
    if (!out)
        return false;
    *out = BastionLayout16();
    const BastionData16& data = bastionData16();
    if (!data.valid)
    {
        if (error)
            *error = data.error;
        return false;
    }

    static const char *startPools[] = {
        "bastion/units/base",
        "bastion/hoglin_stable/origin",
        "bastion/treasure/starters",
        "bastion/bridge/start",
    };
    constexpr int maxDepth = 60;
    constexpr int maximumPieces = 4096;

    uint64_t random = chunkGenerateRnd(worldSeed, chunkX, chunkZ);
    const int startType = nextInt(&random, 4);
    const int startRotation = nextInt(&random, 4);
    const Pool16& startPool =
        data.pools[data.poolByName.value(
            QLatin1String(startPools[startType]))];
    if (startPool.templates.isEmpty())
    {
        if (error)
            *error = QCoreApplication::translate(
                "BastionStructure", "The bastion start pool is empty.");
        return false;
    }
    const int startTemplateIndex =
        startPool.templates[nextInt(
            &random, startPool.templates.size())];

    Piece16 startPiece;
    startPiece.templateIndex = startTemplateIndex;
    startPiece.origin = {chunkX * 16, 33, chunkZ * 16};
    startPiece.rotation = startRotation;
    startPiece.box = templateBox(
        data.templates[startTemplateIndex],
        startPiece.origin, startRotation);

    // JigsawPlacement aligns groundLevelDelta=1 with the requested Y=33.
    startPiece.origin.y--;
    startPiece.box.y0--;
    startPiece.box.y1--;

    QVector<Piece16> pieces;
    pieces.reserve(256);
    pieces.push_back(startPiece);

    const int centerX =
        (startPiece.box.x0 + startPiece.box.x1) / 2;
    const int centerZ =
        (startPiece.box.z0 + startPiece.box.z1) / 2;
    auto globalFree = std::make_shared<FreeRegion>();
    globalFree->outer = {
        double(centerX - 80), double(33 - 80),
        double(centerZ - 80),
        double(centerX + 81), double(33 + 81),
        double(centerZ + 81),
    };
    globalFree->occupied.push_back(halfOpen(startPiece.box));

    std::deque<PieceState> queue;
    queue.push_back({0, globalFree, 0});

    while (!queue.empty())
    {
        const PieceState state = queue.front();
        queue.pop_front();
        const Piece16 parent = pieces[state.pieceIndex];
        const Template16& parentTemplate =
            data.templates[parent.templateIndex];
        QVector<Jigsaw16> sources =
            shuffledJigsaws(parentTemplate, parent, &random);
        std::shared_ptr<FreeRegion> localFree;

        for (const Jigsaw16& source : sources)
        {
            const Point3 outside = add(
                source.pos, directionStep(source.front));
            std::shared_ptr<FreeRegion> selectedFree;
            if (parent.box.contains(outside))
            {
                if (!localFree)
                {
                    localFree = std::make_shared<FreeRegion>();
                    localFree->outer = halfOpen(parent.box);
                }
                selectedFree = localFree;
            }
            else
            {
                selectedFree = state.free;
            }

            QVector<int> candidates;
            const auto primaryPool =
                data.poolByName.constFind(source.pool);
            if (primaryPool == data.poolByName.constEnd())
            {
                // "empty" is the built-in empty pool.
                if (source.pool != QLatin1String("empty"))
                    continue;
            }
            else
            {
                const Pool16& pool = data.pools[*primaryPool];
                if (state.depth != maxDepth)
                    candidates = shuffledPool(
                        data, pool.name, &random);
                const QVector<int> fallback =
                    shuffledPool(data, pool.fallback, &random);
                candidates += fallback;
            }

            bool placed = false;
            for (int candidateTemplateIndex : candidates)
            {
                QVector<int> rotations = {0, 1, 2, 3};
                javaShuffle(&rotations, &random);
                for (int candidateRotation : rotations)
                {
                    Piece16 candidate;
                        candidate.templateIndex =
                            candidateTemplateIndex;
                    candidate.origin = {};
                        candidate.rotation = candidateRotation;
                        candidate.depth = state.depth + 1;
                    const Template16& candidateTemplate =
                        data.templates[candidateTemplateIndex];
                    QVector<Jigsaw16> candidateJigsaws =
                        shuffledJigsaws(
                            candidateTemplate, candidate, &random);
                    for (const Jigsaw16& candidateJigsaw :
                         candidateJigsaws)
                    {
                        if (!canAttach(source, candidateJigsaw))
                            continue;
                        candidate.origin = subtract(
                            outside, candidateJigsaw.pos);
                        candidate.box = templateBox(
                            candidateTemplate, candidate.origin,
                            candidateRotation);
                        if (!reserveBox(
                                selectedFree.get(), candidate.box))
                        {
                            continue;
                        }

                        const int pieceIndex = pieces.size();
                        pieces.push_back(candidate);
                        if (pieces.size() > maximumPieces)
                        {
                            if (error)
                            {
                                *error = QCoreApplication::translate(
                                    "BastionStructure",
                                    "The bastion piece count exceeded the safety limit.");
                            }
                            return false;
                        }
                        if (state.depth + 1 <= maxDepth)
                        {
                            queue.push_back({
                                pieceIndex, selectedFree,
                                state.depth + 1,
                            });
                        }
                        placed = true;
                        break;
                    }
                    if (placed)
                        break;
                }
                if (placed)
                    break;
            }
        }
    }

    QVector<PendingChest> pending;
    for (int pieceIndex = 0;
         pieceIndex < pieces.size(); pieceIndex++)
    {
        const Piece16& piece = pieces[pieceIndex];
        const Template16& structure =
            data.templates[piece.templateIndex];
        for (const Container16& container :
             structure.containers)
        {
            PendingChest generated;
            const Point3 world = add(
                rotatePoint(container.pos, piece.rotation),
                piece.origin);
            generated.chest.pos = {
                world.x, world.y, world.z,
            };
            generated.chest.table = container.table;
            generated.chest.piece = structure.name;
            generated.pieceIndex = pieceIndex;
            generated.placementIndex =
                container.placementIndex;
            pending.push_back(generated);
        }
    }
    assignLootSeeds(&pending, worldSeed);

    out->startType = startType;
    out->rotation = startRotation;
    out->pieceCount = pieces.size();
    if (includePieces)
    {
        out->pieces.reserve(pieces.size());
        for (const Piece16& piece : pieces)
        {
            const Template16& structure =
                data.templates[piece.templateIndex];
            BastionPiece16 generated;
            generated.name = structure.name;
            generated.pos = {
                piece.origin.x, piece.origin.y, piece.origin.z,
            };
            generated.bb0 = {
                piece.box.x0, piece.box.y0, piece.box.z0,
            };
            generated.bb1 = {
                piece.box.x1, piece.box.y1, piece.box.z1,
            };
            generated.rotation = piece.rotation;
            generated.depth = piece.depth;
            out->pieces.push_back(generated);
        }
    }
    out->chests.reserve(pending.size());
    for (const PendingChest& generated : pending)
        out->chests.push_back(generated.chest);
    return true;
}

bool generateBastionLayout16(
    BastionLayout16 *out, uint64_t worldSeed,
    int chunkX, int chunkZ, QString *error)
{
    return generateBastionLayout16Internal(
        out, worldSeed, chunkX, chunkZ, true, error);
}

bool generateBastionLootChests16(
    QVector<BastionLootChest16> *out, uint64_t worldSeed,
    int chunkX, int chunkZ, QString *error)
{
    if (!out)
        return false;
    BastionLayout16 layout;
    if (!generateBastionLayout16Internal(
            &layout, worldSeed, chunkX, chunkZ, false, error))
    {
        out->clear();
        return false;
    }
    *out = std::move(layout.chests);
    return true;
}
