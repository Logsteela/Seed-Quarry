#include "villagestructure.h"

#include "cubiomes/generator.h"
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
#include <array>
#include <deque>
#include <memory>

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

enum ElementKind16
{
    ELEMENT_LEGACY,
    ELEMENT_FEATURE,
    ELEMENT_EMPTY,
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

    int ySpan() const
    {
        return y1 - y0 + 1;
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
    QString block;
    QString lootTable;
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

struct Element16
{
    int kind = ELEMENT_EMPTY;
    int templateIndex = -1;
    QString feature;
    bool terrainMatching = false;
};

struct Pool16
{
    QString name;
    QString fallback;
    bool terrainMatching = false;
    int maxYSpan = 0;
    QVector<Element16> elements;
};

struct Piece16
{
    Element16 element;
    Point3 origin;
    int rotation = 0;
    int depth = 0;
    int groundLevelDelta = 1;
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
    int boundsTop = 0;
    int depth = 0;
};

struct VillageData16
{
    bool valid = false;
    QString path;
    QString error;
    QVector<Template16> templates;
    QVector<Pool16> pools;
    QHash<QString, int> templateByName;
    QHash<QString, int> poolByName;
};

struct HeightReader
{
    VillageHeightCallback16 callback = nullptr;
    void *context = nullptr;
    QHash<qint64, int> cache;
    bool valid = true;

    int get(int x, int z)
    {
        const qint64 key = qint64(
            (quint64(quint32(x)) << 32) |
            quint64(quint32(z)));
        const auto found = cache.constFind(key);
        if (found != cache.constEnd())
            return *found;
        const int height = callback ? callback(context, x, z) : -1;
        if (height < 0)
            valid = false;
        cache.insert(key, height);
        return height;
    }
};

struct CubiomesHeightContext
{
    Generator generator = {};
    SurfaceNoise surfaceNoise = {};
    QHash<qint64, std::array<double, 33>> terrainColumns;

    bool getColumn(int noiseX, int noiseZ, double out[33])
    {
        const qint64 key = qint64(
            (quint64(quint32(noiseX)) << 32) |
            quint64(quint32(noiseZ)));
        auto found = terrainColumns.constFind(key);
        if (found == terrainColumns.constEnd())
        {
            std::array<double, 33> generated;
            if (!getTerrainNoiseColumn116(
                    &generator, &surfaceNoise,
                    noiseX, noiseZ, generated.data()))
                return false;
            terrainColumns.insert(key, generated);
            found = terrainColumns.constFind(key);
        }
        std::copy(found->begin(), found->end(), out);
        return true;
    }
};

QString withoutMinecraftNamespace(QString name)
{
    if (name.startsWith(QLatin1String("minecraft:")))
        name.remove(0, 10);
    return name;
}

QString canonicalTemplateName(QString name)
{
    name = withoutMinecraftNamespace(name);
    if (!name.startsWith(QLatin1String("village/")))
        name.prepend(QLatin1String("village/"));
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
    const QJsonArray array = value.toArray();
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
    if (name == QLatin1String("chests/village/village_armorer"))
        return LOOT_TABLE16_VILLAGE_ARMORER;
    if (name == QLatin1String("chests/village/village_butcher"))
        return LOOT_TABLE16_VILLAGE_BUTCHER;
    if (name == QLatin1String("chests/village/village_cartographer"))
        return LOOT_TABLE16_VILLAGE_CARTOGRAPHER;
    if (name == QLatin1String("chests/village/village_desert_house"))
        return LOOT_TABLE16_VILLAGE_DESERT_HOUSE;
    if (name == QLatin1String("chests/village/village_fisher"))
        return LOOT_TABLE16_VILLAGE_FISHER;
    if (name == QLatin1String("chests/village/village_fletcher"))
        return LOOT_TABLE16_VILLAGE_FLETCHER;
    if (name == QLatin1String("chests/village/village_mason"))
        return LOOT_TABLE16_VILLAGE_MASON;
    if (name == QLatin1String("chests/village/village_plains_house"))
        return LOOT_TABLE16_VILLAGE_PLAINS_HOUSE;
    if (name == QLatin1String("chests/village/village_savanna_house"))
        return LOOT_TABLE16_VILLAGE_SAVANNA_HOUSE;
    if (name == QLatin1String("chests/village/village_shepherd"))
        return LOOT_TABLE16_VILLAGE_SHEPHERD;
    if (name == QLatin1String("chests/village/village_snowy_house"))
        return LOOT_TABLE16_VILLAGE_SNOWY_HOUSE;
    if (name == QLatin1String("chests/village/village_taiga_house"))
        return LOOT_TABLE16_VILLAGE_TAIGA_HOUSE;
    if (name == QLatin1String("chests/village/village_tannery"))
        return LOOT_TABLE16_VILLAGE_TANNERY;
    if (name == QLatin1String("chests/village/village_temple"))
        return LOOT_TABLE16_VILLAGE_TEMPLE;
    if (name == QLatin1String("chests/village/village_toolsmith"))
        return LOOT_TABLE16_VILLAGE_TOOLSMITH;
    if (name == QLatin1String("chests/village/village_weaponsmith"))
        return LOOT_TABLE16_VILLAGE_WEAPONSMITH;
    return -1;
}

QString featureName(const QString& descriptor)
{
    if (descriptor.startsWith(QLatin1String("Feature.TREE.")))
        return QStringLiteral("Feature[minecraft:tree]");
    if (descriptor.startsWith(QLatin1String("Feature.FLOWER.")))
        return QStringLiteral("Feature[minecraft:flower]");
    if (descriptor.startsWith(QLatin1String("Feature.BLOCK_PILE.")))
        return QStringLiteral("Feature[minecraft:block_pile]");
    if (descriptor.startsWith(QLatin1String("Feature.RANDOM_PATCH.")))
        return QStringLiteral("Feature[minecraft:random_patch]");
    return QStringLiteral("Feature[unknown]");
}

QStringList manifestCandidates()
{
    QStringList paths;
    const QString configured =
        qEnvironmentVariable("SEED_ATLAS_STRUCTURE_DATA");
    if (!configured.isEmpty())
    {
        const QFileInfo info(configured);
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
    if (first.y != second.y)
        return first.y < second.y;
    if (first.x != second.x)
        return first.x < second.x;
    if (first.z != second.z)
        return first.z < second.z;
    return firstPlacement < secondPlacement;
}

int elementYSpan(
    const VillageData16& data, const Element16& element)
{
    if (element.kind == ELEMENT_LEGACY &&
        element.templateIndex >= 0 &&
        element.templateIndex < data.templates.size())
    {
        return data.templates[element.templateIndex].size.y;
    }
    if (element.kind == ELEMENT_FEATURE)
        return 1;
    /*
     * EmptyPoolElement#getBoundingBox returns BoundingBox.getUnknownBox().
     * Its Java int-overflowing getYSpan() is 2.
     */
    return 2;
}

VillageData16 loadVillageData16()
{
    VillageData16 data;
    for (const QString& candidate : manifestCandidates())
    {
        const QFileInfo info(candidate);
        if (info.isFile())
        {
            data.path = info.absoluteFilePath();
            break;
        }
    }
    if (data.path.isEmpty())
    {
        data.error = QStringLiteral(
            "Java 1.16.1 jigsaw-1.16.1.json was not found.");
        return data;
    }

    QFile file(data.path);
    if (!file.open(QIODevice::ReadOnly))
    {
        data.error = QStringLiteral(
            "Could not read Java 1.16.1 structure data: ") +
            file.errorString();
        return data;
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject())
    {
        data.error = QStringLiteral(
            "Java 1.16.1 structure data is invalid: ") +
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
        data.error = QStringLiteral(
            "Structure data does not match the official Java 1.16.1 jar.");
        return data;
    }

    bool ok = true;
    int containerCount = 0;
    int jigsawCount = 0;
    const QJsonObject sourceSha1 = root
        .value(QStringLiteral("pools")).toObject()
        .value(QStringLiteral("village")).toObject()
        .value(QStringLiteral("source_sha1")).toObject();
    static const struct {
        const char *file;
        const char *sha1;
    } expectedSources[] = {
        {"DesertVillagePools.java",
         "cd1987546e276f333caf1a5ca93deef52847d9f0"},
        {"PlainVillagePools.java",
         "485df203785174eb2b6767e8dd5a4394cb58cf13"},
        {"SavannaVillagePools.java",
         "eb749c45c2a97ac76bd165bc9d0080c9a08b43fd"},
        {"SnowyVillagePools.java",
         "eff78072fcbf67d8c1dc806ba370b4e8912c1489"},
        {"TaigaVillagePools.java",
         "b4918e50d9ac65ac847f87ad5b5355cefbb61052"},
    };
    if (sourceSha1.size() !=
        int(sizeof(expectedSources) / sizeof(expectedSources[0])))
    {
        ok = false;
    }
    for (const auto& expected : expectedSources)
    {
        if (sourceSha1.value(
                QLatin1String(expected.file)).toString() !=
            QLatin1String(expected.sha1))
        {
            ok = false;
        }
    }
    const QJsonArray structures = root
        .value(QStringLiteral("structures")).toObject()
        .value(QStringLiteral("village")).toArray();
    data.templates.reserve(structures.size());
    for (const QJsonValue& value : structures)
    {
        const QJsonObject object = value.toObject();
        Template16 structure;
        structure.name = canonicalTemplateName(
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
            jigsawCount++;
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
            container.block = withoutMinecraftNamespace(
                containerObject.value(
                    QStringLiteral("block")).toString());
            container.lootTable = withoutMinecraftNamespace(
                containerObject.value(
                    QStringLiteral("loot_table")).toString());
            container.table = container.lootTable.isEmpty()
                ? -1 : tableFromName(container.lootTable);
            container.placementIndex = containerObject.value(
                QStringLiteral("placement_index")).toInt(-1);
            if (container.block.isEmpty() ||
                (!container.lootTable.isEmpty() &&
                 container.table < 0) ||
                container.placementIndex < 0)
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

    int rawCount = 0;
    int expandedCount = 0;
    int legacyCount = 0;
    int featureCount = 0;
    int emptyCount = 0;
    const QJsonArray definitions = root
        .value(QStringLiteral("pools")).toObject()
        .value(QStringLiteral("village")).toObject()
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
        const QString projection =
            object.value(QStringLiteral("projection")).toString();
        pool.terrainMatching =
            projection == QLatin1String("terrain_matching");
        if (pool.name.isEmpty() || pool.fallback.isEmpty() ||
            data.poolByName.contains(pool.name) ||
            (!pool.terrainMatching &&
             projection != QLatin1String("rigid")))
        {
            ok = false;
            break;
        }

        const QJsonArray elements =
            object.value(QStringLiteral("elements")).toArray();
        for (const QJsonValue& elementValue : elements)
        {
            const QJsonObject objectElement = elementValue.toObject();
            Element16 element;
            element.terrainMatching = pool.terrainMatching;
            const QString type =
                objectElement.value(QStringLiteral("type")).toString();
            const int weight =
                objectElement.value(QStringLiteral("weight")).toInt();
            if (weight <= 0 || weight > 1000)
            {
                ok = false;
                break;
            }
            if (type == QLatin1String("legacy"))
            {
                const QString templateName = canonicalTemplateName(
                    objectElement.value(
                        QStringLiteral("template")).toString());
                const auto found =
                    data.templateByName.constFind(templateName);
                if (found == data.templateByName.constEnd())
                {
                    ok = false;
                    break;
                }
                element.kind = ELEMENT_LEGACY;
                element.templateIndex = *found;
                legacyCount++;
            }
            else if (type == QLatin1String("feature"))
            {
                element.kind = ELEMENT_FEATURE;
                element.feature = objectElement.value(
                    QStringLiteral("feature")).toString();
                if (element.feature.isEmpty())
                {
                    ok = false;
                    break;
                }
                featureCount++;
            }
            else if (type == QLatin1String("empty"))
            {
                element.kind = ELEMENT_EMPTY;
                emptyCount++;
            }
            else
            {
                ok = false;
                break;
            }
            rawCount++;
            expandedCount += weight;
            for (int occurrence = 0; occurrence < weight; occurrence++)
                pool.elements.push_back(element);
        }
        if (!ok)
            break;
        data.poolByName.insert(pool.name, data.pools.size());
        data.pools.push_back(pool);
    }

    for (Pool16& pool : data.pools)
    {
        for (const Element16& element : pool.elements)
            pool.maxYSpan = std::max(
                pool.maxYSpan, elementYSpan(data, element));
    }

    static const char *startPools[] = {
        "village/plains/town_centers",
        "village/desert/town_centers",
        "village/savanna/town_centers",
        "village/snowy/town_centers",
        "village/taiga/town_centers",
    };
    for (const char *startPool : startPools)
    {
        if (!data.poolByName.contains(QLatin1String(startPool)))
            ok = false;
    }
    for (const Template16& structure : data.templates)
    {
        for (const Jigsaw16& jigsaw : structure.jigsaws)
        {
            if (jigsaw.pool != QLatin1String("empty") &&
                !data.poolByName.contains(jigsaw.pool))
            {
                ok = false;
            }
        }
    }

    if (!ok || data.templates.size() != 482 ||
        jigsawCount != 1871 ||
        data.pools.size() != 61 || containerCount != 80 ||
        rawCount != 648 || expandedCount != 2949 ||
        legacyCount != 591 || featureCount != 35 ||
        emptyCount != 22)
    {
        data.error = QStringLiteral(
            "Structure data does not contain the expected official "
            "Java 1.16.1 Village registry.");
        data.templates.clear();
        data.pools.clear();
        data.templateByName.clear();
        data.poolByName.clear();
        return data;
    }

    data.valid = true;
    return data;
}

const VillageData16& villageData16()
{
    static const VillageData16 data = loadVillageData16();
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

Box3 elementBox(
    const VillageData16& data, const Element16& element,
    const Point3& origin, int rotation)
{
    if (element.kind == ELEMENT_LEGACY)
    {
        return templateBox(
            data.templates[element.templateIndex], origin, rotation);
    }
    if (element.kind == ELEMENT_FEATURE)
    {
        return {
            origin.x, origin.y, origin.z,
            origin.x, origin.y, origin.z,
        };
    }
    return {
        1, 1, 1, 0, 0, 0,
    };
}

void moveBox(Box3 *box, int x, int y, int z)
{
    box->x0 += x;
    box->x1 += x;
    box->y0 += y;
    box->y1 += y;
    box->z0 += z;
    box->z1 += z;
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
    const HalfOpenBox candidate = {
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
    const VillageData16& data, const Element16& element,
    const Point3& origin, int rotation, uint64_t *random)
{
    QVector<Jigsaw16> result;
    if (element.kind == ELEMENT_LEGACY)
    {
        result = data.templates[element.templateIndex].jigsaws;
    }
    else if (element.kind == ELEMENT_FEATURE)
    {
        Jigsaw16 jigsaw;
        jigsaw.front = DIR_DOWN;
        jigsaw.top = DIR_SOUTH;
        jigsaw.pool = QStringLiteral("empty");
        jigsaw.target = QStringLiteral("empty");
        jigsaw.name = QStringLiteral("bottom");
        jigsaw.rollable = true;
        result.push_back(jigsaw);
    }
    for (Jigsaw16& jigsaw : result)
    {
        jigsaw.pos = add(
            rotatePoint(jigsaw.pos, rotation), origin);
        if (element.kind == ELEMENT_LEGACY)
        {
            jigsaw.front =
                rotateDirection(jigsaw.front, rotation);
            jigsaw.top =
                rotateDirection(jigsaw.top, rotation);
        }
    }
    javaShuffle(&result, random);
    return result;
}

QVector<Element16> shuffledPool(
    const VillageData16& data, const QString& name,
    uint64_t *random)
{
    const auto found = data.poolByName.constFind(name);
    if (found == data.poolByName.constEnd())
        return {};
    QVector<Element16> result = data.pools[*found].elements;
    javaShuffle(&result, random);
    /*
     * fastutil ObjectArrays.shuffle includes nextInt(1) for a one-or-more
     * element array. Collections.shuffle, used elsewhere, does not.
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

int poolMaxYSpan(
    const VillageData16& data, const QString& poolName)
{
    const auto found = data.poolByName.constFind(poolName);
    if (found == data.poolByName.constEnd())
        return 0;
    return data.pools[*found].maxYSpan;
}

int expansionHeight(
    const VillageData16& data, const Element16& element,
    const QVector<Jigsaw16>& jigsaws, const Box3& box)
{
    if (box.ySpan() > 16)
        return 0;
    int expansion = 0;
    for (const Jigsaw16& jigsaw : jigsaws)
    {
        const Point3 outside = add(
            jigsaw.pos, directionStep(jigsaw.front));
        if (!box.contains(outside))
            continue;
        const auto primary = data.poolByName.constFind(jigsaw.pool);
        if (primary == data.poolByName.constEnd())
            continue;
        const Pool16& pool = data.pools[*primary];
        expansion = std::max(
            expansion,
            std::max(
                pool.maxYSpan,
                poolMaxYSpan(data, pool.fallback)));
    }
    (void) element;
    return expansion;
}

bool villageTypeForBiome(
    int biomeId, int *type, QString *startPool)
{
    switch (biomeId)
    {
    case plains:
        *type = VillageLayout16::PLAINS;
        *startPool = QStringLiteral(
            "village/plains/town_centers");
        return true;
    case desert:
        *type = VillageLayout16::DESERT;
        *startPool = QStringLiteral(
            "village/desert/town_centers");
        return true;
    case savanna:
        *type = VillageLayout16::SAVANNA;
        *startPool = QStringLiteral(
            "village/savanna/town_centers");
        return true;
    case snowy_tundra:
        *type = VillageLayout16::SNOWY;
        *startPool = QStringLiteral(
            "village/snowy/town_centers");
        return true;
    case taiga:
        *type = VillageLayout16::TAIGA;
        *startPool = QStringLiteral(
            "village/taiga/town_centers");
        return true;
    default:
        return false;
    }
}

int cubiomesHeight(void *context, int blockX, int blockZ)
{
    CubiomesHeightContext *height =
        static_cast<CubiomesHeightContext *>(context);
    const int cellX = floordiv(blockX, 4);
    const int cellZ = floordiv(blockZ, 4);
    double columns[4][33];
    if (!height->getColumn(
            cellX, cellZ, columns[0]) ||
        !height->getColumn(
            cellX, cellZ + 1, columns[1]) ||
        !height->getColumn(
            cellX + 1, cellZ, columns[2]) ||
        !height->getColumn(
            cellX + 1, cellZ + 1, columns[3]))
    {
        return -1;
    }
    return getFirstFreeHeightFromColumns116(
        (const double (*)[33]) columns, blockX, blockZ);
}

}

bool isVillageStructureData16Available(QString *error)
{
    const VillageData16& data = villageData16();
    if (error)
        *error = data.error;
    return data.valid;
}

QString villageStructureData16Path()
{
    return villageData16().path;
}

bool generateVillageLayout16WithHeights(
    VillageLayout16 *out, uint64_t worldSeed,
    int chunkX, int chunkZ, int biomeId,
    VillageHeightCallback16 heightCallback, void *heightContext,
    QString *error)
{
    if (!out)
        return false;
    *out = VillageLayout16();
    const VillageData16& data = villageData16();
    if (!data.valid)
    {
        if (error)
            *error = data.error;
        return false;
    }
    if (!heightCallback)
    {
        if (error)
            *error = QStringLiteral(
                "A WORLD_SURFACE_WG height callback is required.");
        return false;
    }

    int villageType;
    QString startPoolName;
    if (!villageTypeForBiome(
            biomeId, &villageType, &startPoolName))
    {
        if (error)
            *error = QStringLiteral(
                "The biome is not a Java 1.16.1 Village biome.");
        return false;
    }

    constexpr int maxDepth = 6;
    constexpr int maximumPieces = 4096;
    HeightReader heights;
    heights.callback = heightCallback;
    heights.context = heightContext;

    uint64_t random = chunkGenerateRnd(worldSeed, chunkX, chunkZ);
    const int startRotation = nextInt(&random, 4);
    const Pool16& startPool =
        data.pools[data.poolByName.value(startPoolName)];
    if (startPool.elements.isEmpty())
    {
        if (error)
            *error = QStringLiteral("The Village start pool is empty.");
        return false;
    }
    const Element16 startElement =
        startPool.elements[nextInt(
            &random, startPool.elements.size())];
    if (startElement.kind != ELEMENT_LEGACY)
    {
        if (error)
            *error = QStringLiteral(
                "The Village start pool selected a non-template element.");
        return false;
    }

    Piece16 startPiece;
    startPiece.element = startElement;
    startPiece.origin = {chunkX * 16, 0, chunkZ * 16};
    startPiece.rotation = startRotation;
    startPiece.box = elementBox(
        data, startElement, startPiece.origin, startRotation);
    const int centerX =
        (startPiece.box.x0 + startPiece.box.x1) / 2;
    const int centerZ =
        (startPiece.box.z0 + startPiece.box.z1) / 2;
    const int startHeight = heights.get(centerX, centerZ);
    if (!heights.valid)
    {
        if (error)
            *error = QStringLiteral(
                "WORLD_SURFACE_WG height generation failed.");
        return false;
    }

    const int startGroundY =
        startPiece.box.y0 + startPiece.groundLevelDelta;
    const int startMoveY = startHeight - startGroundY;
    startPiece.origin.y += startMoveY;
    moveBox(&startPiece.box, 0, startMoveY, 0);

    QVector<Piece16> pieces;
    pieces.reserve(256);
    pieces.push_back(startPiece);

    auto globalFree = std::make_shared<FreeRegion>();
    globalFree->outer = {
        double(centerX - 80), double(startHeight - 80),
        double(centerZ - 80),
        double(centerX + 81), double(startHeight + 81),
        double(centerZ + 81),
    };
    globalFree->occupied.push_back(halfOpen(startPiece.box));

    std::deque<PieceState> queue;
    queue.push_back({
        0, globalFree, startHeight + 80, 0,
    });

    while (!queue.empty())
    {
        const PieceState state = queue.front();
        queue.pop_front();
        const Piece16 parent = pieces[state.pieceIndex];
        const bool parentRigid =
            !parent.element.terrainMatching;
        QVector<Jigsaw16> sources = shuffledJigsaws(
            data, parent.element, parent.origin,
            parent.rotation, &random);
        std::shared_ptr<FreeRegion> localFree;

        for (const Jigsaw16& source : sources)
        {
            const Point3 outside = add(
                source.pos, directionStep(source.front));
            std::shared_ptr<FreeRegion> selectedFree;
            int childBoundsTop;
            if (parent.box.contains(outside))
            {
                if (!localFree)
                {
                    localFree = std::make_shared<FreeRegion>();
                    localFree->outer = halfOpen(parent.box);
                }
                selectedFree = localFree;
                childBoundsTop = parent.box.y0;
            }
            else
            {
                selectedFree = state.free;
                childBoundsTop = state.boundsTop;
            }

            QVector<Element16> candidates;
            const auto primaryFound =
                data.poolByName.constFind(source.pool);
            if (primaryFound == data.poolByName.constEnd())
            {
                if (source.pool != QLatin1String("empty"))
                    continue;
            }
            else
            {
                const Pool16& primary = data.pools[*primaryFound];
                if (state.depth != maxDepth)
                {
                    candidates = shuffledPool(
                        data, primary.name, &random);
                }
                candidates += shuffledPool(
                    data, primary.fallback, &random);
            }

            const int sourceYInParent =
                source.pos.y - parent.box.y0;
            int sourceSurfaceHeight = -1;
            bool placed = false;
            for (const Element16& candidateElement : candidates)
            {
                if (candidateElement.kind == ELEMENT_EMPTY)
                    break;

                QVector<int> rotations = {0, 1, 2, 3};
                javaShuffle(&rotations, &random);
                for (int candidateRotation : rotations)
                {
                    const Point3 zero = {};
                    QVector<Jigsaw16> candidateJigsaws =
                        shuffledJigsaws(
                            data, candidateElement, zero,
                            candidateRotation, &random);
                    const Box3 zeroBox = elementBox(
                        data, candidateElement, zero,
                        candidateRotation);
                    const int expansion = expansionHeight(
                        data, candidateElement,
                        candidateJigsaws, zeroBox);

                    for (const Jigsaw16& candidateJigsaw :
                         candidateJigsaws)
                    {
                        if (!canAttach(source, candidateJigsaw))
                            continue;

                        Piece16 candidate;
                        candidate.element = candidateElement;
                        candidate.rotation = candidateRotation;
                        candidate.depth = state.depth + 1;
                        candidate.origin = subtract(
                            outside, candidateJigsaw.pos);
                        candidate.box = elementBox(
                            data, candidateElement,
                            candidate.origin, candidateRotation);
                        const int candidateBoxY0 = candidate.box.y0;
                        const bool candidateRigid =
                            !candidateElement.terrainMatching;
                        const int verticalOffset =
                            sourceYInParent -
                            candidateJigsaw.pos.y +
                            directionStep(source.front).y;
                        int targetY;
                        if (parentRigid && candidateRigid)
                        {
                            targetY =
                                parent.box.y0 + verticalOffset;
                        }
                        else
                        {
                            if (sourceSurfaceHeight < 0)
                            {
                                sourceSurfaceHeight = heights.get(
                                    source.pos.x, source.pos.z);
                            }
                            targetY =
                                sourceSurfaceHeight -
                                candidateJigsaw.pos.y;
                        }
                        if (!heights.valid)
                        {
                            if (error)
                            {
                                *error = QStringLiteral(
                                    "WORLD_SURFACE_WG height "
                                    "generation failed.");
                            }
                            return false;
                        }

                        const int moveY =
                            targetY - candidateBoxY0;
                        candidate.origin.y += moveY;
                        moveBox(&candidate.box, 0, moveY, 0);
                        if (expansion > 0)
                        {
                            const int expandedDifference = std::max(
                                expansion + 1,
                                candidate.box.y1 -
                                candidate.box.y0);
                            candidate.box.y1 =
                                candidate.box.y0 +
                                expandedDifference;
                        }
                        if (!reserveBox(
                                selectedFree.get(), candidate.box))
                        {
                            continue;
                        }

                        candidate.groundLevelDelta =
                            candidateRigid
                            ? parent.groundLevelDelta -
                                verticalOffset
                            : 1;
                        const int pieceIndex = pieces.size();
                        pieces.push_back(candidate);
                        if (pieces.size() > maximumPieces)
                        {
                            if (error)
                            {
                                *error = QStringLiteral(
                                    "Village piece safety limit "
                                    "was exceeded.");
                            }
                            return false;
                        }
                        if (state.depth + 1 <= maxDepth)
                        {
                            queue.push_back({
                                pieceIndex, selectedFree,
                                childBoundsTop,
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

    out->villageType = villageType;
    out->biome = biomeId;
    out->rotation = startRotation;
    out->startPool = startPoolName;
    out->pieceCount = pieces.size();
    out->pieces.reserve(pieces.size());
    for (const Piece16& piece : pieces)
    {
        VillagePiece16 generated;
        if (piece.element.kind == ELEMENT_LEGACY)
        {
            generated.name =
                data.templates[piece.element.templateIndex].name;
            generated.elementType =
                VillagePiece16::LEGACY_TEMPLATE;
        }
        else
        {
            generated.name = featureName(piece.element.feature);
            generated.feature = piece.element.feature;
            generated.elementType = VillagePiece16::FEATURE;
        }
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
        generated.groundLevelDelta =
            piece.groundLevelDelta;
        generated.terrainMatching =
            piece.element.terrainMatching;
        out->pieces.push_back(generated);
    }

    for (int pieceIndex = 0;
         pieceIndex < pieces.size(); pieceIndex++)
    {
        const Piece16& piece = pieces[pieceIndex];
        if (piece.element.kind != ELEMENT_LEGACY)
            continue;
        const Template16& structure =
            data.templates[piece.element.templateIndex];
        for (const Container16& container :
             structure.containers)
        {
            const Point3 transformed =
                rotatePoint(container.pos, piece.rotation);
            Point3 world = add(transformed, piece.origin);
            if (piece.element.terrainMatching)
            {
                world.y = heights.get(world.x, world.z) -
                    1 + container.pos.y;
                if (!heights.valid)
                {
                    if (error)
                    {
                        *error = QStringLiteral(
                            "WORLD_SURFACE_WG height "
                            "generation failed.");
                    }
                    return false;
                }
            }

            VillageContainer16 generated;
            generated.pos = {world.x, world.y, world.z};
            generated.block = container.block;
            generated.lootTable = container.lootTable;
            generated.table = container.table;
            generated.pieceIndex = pieceIndex;
            generated.placementIndex =
                container.placementIndex;
            generated.piece = structure.name;
            out->containers.push_back(generated);
        }
    }
    return true;
}

bool generateVillageLayout16(
    VillageLayout16 *out, uint64_t worldSeed,
    int chunkX, int chunkZ, int biomeId,
    QString *error)
{
    CubiomesHeightContext height;
    setupGenerator(&height.generator, MC_1_16_1, 0);
    applySeed(
        &height.generator, DIM_OVERWORLD, worldSeed);
    initSurfaceNoise(
        &height.surfaceNoise, DIM_OVERWORLD, worldSeed);
    return generateVillageLayout16WithHeights(
        out, worldSeed, chunkX, chunkZ, biomeId,
        cubiomesHeight, &height, error);
}
