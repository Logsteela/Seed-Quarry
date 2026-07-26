import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import nl.jellejurre.seedchecker.SeedChecker;
import nl.jellejurre.seedchecker.SeedCheckerDimension;

import net.minecraft.block.entity.BarrelBlockEntity;
import net.minecraft.block.entity.BlockEntity;
import net.minecraft.block.entity.ChestBlockEntity;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.structure.PoolStructurePiece;
import net.minecraft.structure.StructurePiece;
import net.minecraft.structure.StructureStart;
import net.minecraft.util.BlockRotation;
import net.minecraft.util.math.BlockBox;
import net.minecraft.util.math.BlockPos;
import net.minecraft.world.chunk.ProtoChunk;
import net.minecraft.world.gen.feature.StructureFeature;

import java.io.IOException;
import java.io.Reader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;

/**
 * Development-only oracle for Minecraft Java 1.16.1 village starts.
 *
 * <p>The published SeedChecker 1.16.1 jar embeds Minecraft's mapped world
 * generator.  Running it at generation level 1 creates structure starts and
 * their jigsaw pieces, without placing blocks or starting Minecraft's GUI.
 * This program intentionally contains no SeedChecker or Minecraft source.</p>
 */
public final class VillageOracle1161 {
    private static final long REGION_X_MULTIPLIER = 341873128712L;
    private static final long REGION_Z_MULTIPLIER = 132897987541L;
    private static final long VILLAGE_SALT = 10387312L;
    private static final int VILLAGE_SPACING = 32;
    private static final int VILLAGE_RANGE = 24;

    private VillageOracle1161() {}

    private static final class ContainerTemplate {
        final int x;
        final int y;
        final int z;
        final int placementIndex;
        final String block;
        final String lootTable;

        ContainerTemplate(
                int x, int y, int z, int placementIndex,
                String block, String lootTable) {
            this.x = x;
            this.y = y;
            this.z = z;
            this.placementIndex = placementIndex;
            this.block = block;
            this.lootTable = lootTable;
        }
    }

    private static final class ExpectedContainer {
        final int pieceIndex;
        final String template;
        final ContainerTemplate container;
        final BlockPos pos;

        ExpectedContainer(
                int pieceIndex, String template,
                ContainerTemplate container, BlockPos pos) {
            this.pieceIndex = pieceIndex;
            this.template = template;
            this.container = container;
            this.pos = pos;
        }
    }

    private static final class ChunkTarget {
        final int x;
        final int z;

        ChunkTarget(int x, int z) {
            this.x = x;
            this.z = z;
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof ChunkTarget))
                return false;
            ChunkTarget chunk = (ChunkTarget) other;
            return x == chunk.x && z == chunk.z;
        }

        @Override
        public int hashCode() {
            return 31 * x + z;
        }
    }

    private static String oneLine(String value) {
        return value
            .replace('\r', ' ')
            .replace('\n', ' ')
            .replace('|', '/');
    }

    private static boolean printVillage(
            SeedChecker checker, long seed, int chunkX, int chunkZ) {
        ProtoChunk chunk = checker.getChunk(chunkX, chunkZ, 1);
        StructureStart<?> start =
            chunk.getStructureStart(StructureFeature.VILLAGE);
        if (start == null || !start.hasChildren())
            return false;

        List<StructurePiece> pieces = start.getChildren();
        BlockBox total = start.getBoundingBox();
        System.out.printf(
            "V|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d%n",
            seed, chunkX, chunkZ, pieces.size(),
            total.minX, total.minY, total.minZ,
            total.maxX, total.maxY, total.maxZ);

        for (int index = 0; index < pieces.size(); ++index) {
            StructurePiece rawPiece = pieces.get(index);
            BlockBox box = rawPiece.getBoundingBox();
            if (rawPiece instanceof PoolStructurePiece) {
                PoolStructurePiece piece =
                    (PoolStructurePiece) rawPiece;
                BlockPos pos = piece.getPos();
                CompoundTag tag = piece.getTag();
                CompoundTag poolElement =
                    tag.getCompound("pool_element");
                String elementType =
                    poolElement.getString("element_type");
                String element =
                    poolElement.getString("location");
                if (element.isEmpty() &&
                    poolElement.contains("feature", 10)) {
                    element = "feature:" + poolElement
                        .getCompound("feature").getString("name");
                }
                if (element.isEmpty())
                    element = oneLine(
                        piece.getPoolElement().toString());
                System.out.printf(
                    "P|%d|%s|%s|%d|%d|%d|%s|%d|"
                        + "%d|%d|%d|%d|%d|%d%n",
                    index, elementType, oneLine(element),
                    pos.getX(), pos.getY(), pos.getZ(),
                    piece.getRotation().name(),
                    piece.getGroundLevelDelta(),
                    box.minX, box.minY, box.minZ,
                    box.maxX, box.maxY, box.maxZ);
            } else {
                System.out.printf(
                    "P|%d|%s||||||%d|%d|%d|%d|%d|%d%n",
                    index, oneLine(rawPiece.toString()),
                    box.minX, box.minY, box.minZ,
                    box.maxX, box.maxY, box.maxZ);
            }
        }
        return true;
    }

    private static String normalizeTemplate(String template) {
        String value = template;
        if (value.startsWith("minecraft:"))
            value = value.substring("minecraft:".length());
        if (value.startsWith("village/"))
            value = value.substring("village/".length());
        return value;
    }

    private static String namespaced(String identifier) {
        if (identifier == null || identifier.isEmpty())
            return "";
        return identifier.indexOf(':') >= 0
            ? identifier
            : "minecraft:" + identifier;
    }

    private static Map<String, List<ContainerTemplate>>
            loadContainerTemplates(Path manifest) throws IOException {
        JsonObject root;
        try (Reader reader = Files.newBufferedReader(
                manifest, StandardCharsets.UTF_8)) {
            root = new JsonParser().parse(reader).getAsJsonObject();
        }
        JsonElement version = root.get("minecraft_version");
        if (version == null ||
                !"1.16.1".equals(version.getAsString())) {
            throw new IllegalArgumentException(
                "manifest is not for Minecraft 1.16.1: "
                + manifest);
        }

        JsonObject structures = root.getAsJsonObject("structures");
        if (structures == null)
            throw new IllegalArgumentException(
                "manifest has no structures object: " + manifest);
        JsonArray village = structures.getAsJsonArray("village");
        if (village == null)
            throw new IllegalArgumentException(
                "manifest has no structures.village array: " + manifest);

        Map<String, List<ContainerTemplate>> result =
            new HashMap<>();
        for (JsonElement templateElement : village) {
            JsonObject template = templateElement.getAsJsonObject();
            JsonElement containersElement = template.get("containers");
            if (containersElement == null ||
                    !containersElement.isJsonArray())
                continue;

            String name = normalizeTemplate(
                template.get("name").getAsString());
            List<ContainerTemplate> containers = new ArrayList<>();
            for (JsonElement containerElement :
                    containersElement.getAsJsonArray()) {
                JsonObject container =
                    containerElement.getAsJsonObject();
                JsonArray pos = container.getAsJsonArray("pos");
                JsonElement lootElement =
                    container.get("loot_table");
                String lootTable =
                    lootElement == null || lootElement.isJsonNull()
                    ? ""
                    : lootElement.getAsString();
                containers.add(new ContainerTemplate(
                    pos.get(0).getAsInt(),
                    pos.get(1).getAsInt(),
                    pos.get(2).getAsInt(),
                    container.get("placement_index").getAsInt(),
                    container.get("block").getAsString(),
                    lootTable));
            }
            result.put(name, containers);
        }
        return result;
    }

    private static BlockPos transformContainer(
            BlockPos origin, BlockRotation rotation,
            ContainerTemplate container) {
        int x = container.x;
        int z = container.z;
        int rotatedX;
        int rotatedZ;
        switch (rotation) {
        case CLOCKWISE_90:
            rotatedX = -z;
            rotatedZ = x;
            break;
        case CLOCKWISE_180:
            rotatedX = -x;
            rotatedZ = -z;
            break;
        case COUNTERCLOCKWISE_90:
            rotatedX = z;
            rotatedZ = -x;
            break;
        default:
            rotatedX = x;
            rotatedZ = z;
            break;
        }
        return origin.add(rotatedX, container.y, rotatedZ);
    }

    private static List<ExpectedContainer> expectedContainers(
            StructureStart<?> start,
            Map<String, List<ContainerTemplate>> templates) {
        List<ExpectedContainer> result = new ArrayList<>();
        List<StructurePiece> pieces = start.getChildren();
        for (int pieceIndex = 0;
                pieceIndex < pieces.size(); ++pieceIndex) {
            StructurePiece rawPiece = pieces.get(pieceIndex);
            if (!(rawPiece instanceof PoolStructurePiece))
                continue;
            PoolStructurePiece piece =
                (PoolStructurePiece) rawPiece;
            CompoundTag poolElement = piece.getTag()
                .getCompound("pool_element");
            String fullTemplate =
                poolElement.getString("location");
            if (fullTemplate.isEmpty())
                continue;
            String template =
                normalizeTemplate(fullTemplate);
            List<ContainerTemplate> containers =
                templates.get(template);
            if (containers == null)
                continue;
            if (!"rigid".equals(
                    poolElement.getString("projection"))) {
                throw new IllegalArgumentException(
                    "container template is not rigid: "
                    + fullTemplate);
            }
            for (ContainerTemplate container : containers) {
                result.add(new ExpectedContainer(
                    pieceIndex, fullTemplate, container,
                    transformContainer(
                        piece.getPos(), piece.getRotation(),
                        container)));
            }
        }
        return result;
    }

    private static boolean isVillageContainer(
            BlockEntity blockEntity) {
        return blockEntity instanceof ChestBlockEntity ||
            blockEntity instanceof BarrelBlockEntity;
    }

    private static String containerType(BlockEntity blockEntity) {
        if (blockEntity instanceof ChestBlockEntity)
            return "chest";
        if (blockEntity instanceof BarrelBlockEntity)
            return "barrel";
        return blockEntity.getClass().getSimpleName();
    }

    private static boolean insideAnyPiece(
            BlockPos pos, List<StructurePiece> pieces) {
        for (StructurePiece piece : pieces) {
            BlockBox box = piece.getBoundingBox();
            if (pos.getX() >= box.minX && pos.getX() <= box.maxX &&
                    pos.getY() >= box.minY &&
                    pos.getY() <= box.maxY &&
                    pos.getZ() >= box.minZ &&
                    pos.getZ() <= box.maxZ)
                return true;
        }
        return false;
    }

    private static boolean printVillageLoot(
            long seed, int chunkX, int chunkZ,
            Path manifest) throws IOException {
        Map<String, List<ContainerTemplate>> templates =
            loadContainerTemplates(manifest);

        // A target of 8 is required for block entities, but the first call
        // explicitly stops at STRUCTURE_STARTS (level 1).  FEATURES are then
        // generated only for chunks containing expected village containers.
        SeedChecker checker = new SeedChecker(
            seed, 8, SeedCheckerDimension.OVERWORLD);
        ProtoChunk startChunk =
            checker.getChunk(chunkX, chunkZ, 1);
        StructureStart<?> start = startChunk.getStructureStart(
            StructureFeature.VILLAGE);
        if (start == null || !start.hasChildren())
            return false;

        List<StructurePiece> pieceSnapshot =
            new ArrayList<>(start.getChildren());
        List<ExpectedContainer> expected =
            expectedContainers(start, templates);
        Set<ChunkTarget> targets = new LinkedHashSet<>();
        for (ExpectedContainer container : expected) {
            targets.add(new ChunkTarget(
                Math.floorDiv(container.pos.getX(), 16),
                Math.floorDiv(container.pos.getZ(), 16)));
        }

        System.out.printf(
            "C|%d|%d|%d|pieces=%d|containers=%d|chunks=%d%n",
            seed, chunkX, chunkZ, start.getChildren().size(),
            expected.size(), targets.size());

        Map<BlockPos, BlockEntity> generated =
            new LinkedHashMap<>();
        for (ChunkTarget target : targets) {
            ProtoChunk chunk =
                checker.getChunk(target.x, target.z, 8);
            generated.putAll(chunk.getBlockEntities());
        }

        Set<BlockPos> expectedPositions = new LinkedHashSet<>();
        int found = 0;
        int missing = 0;
        for (ExpectedContainer container : expected) {
            expectedPositions.add(container.pos);
            BlockEntity blockEntity =
                generated.get(container.pos);
            String expectedLoot =
                namespaced(container.container.lootTable);
            if (blockEntity == null ||
                    !isVillageContainer(blockEntity)) {
                ++missing;
                System.out.printf(
                    "M|%d|%s|%d|%s|%d|%d|%d|%s%n",
                    container.pieceIndex,
                    oneLine(container.template),
                    container.container.placementIndex,
                    container.container.block,
                    container.pos.getX(),
                    container.pos.getY(),
                    container.pos.getZ(),
                    expectedLoot);
                continue;
            }

            ++found;
            CompoundTag tag =
                blockEntity.toTag(new CompoundTag());
            String actualLoot = tag.getString("LootTable");
            boolean hasSeed =
                tag.contains("LootTableSeed");
            long lootSeed = tag.getLong("LootTableSeed");
            System.out.printf(
                "L|%d|%s|%d|%s|%d|%d|%d|%s|%s|%s|%d|%s%n",
                container.pieceIndex,
                oneLine(container.template),
                container.container.placementIndex,
                container.container.block,
                container.pos.getX(),
                container.pos.getY(),
                container.pos.getZ(),
                containerType(blockEntity),
                expectedLoot,
                oneLine(actualLoot),
                lootSeed,
                hasSeed ? "seed" : "no-seed");
        }

        int unexpected = 0;
        for (Map.Entry<BlockPos, BlockEntity> entry :
                generated.entrySet()) {
            if (!isVillageContainer(entry.getValue()) ||
                    expectedPositions.contains(entry.getKey()) ||
                    !insideAnyPiece(
                        entry.getKey(), pieceSnapshot))
                continue;
            ++unexpected;
            CompoundTag tag =
                entry.getValue().toTag(new CompoundTag());
            BlockPos pos = entry.getKey();
            System.out.printf(
                "U|%s|%d|%d|%d|%s|%d|%s%n",
                containerType(entry.getValue()),
                pos.getX(), pos.getY(), pos.getZ(),
                oneLine(tag.getString("LootTable")),
                tag.getLong("LootTableSeed"),
                tag.contains("LootTableSeed")
                    ? "seed" : "no-seed");
        }
        System.out.printf(
            "E|found=%d|missing=%d|unexpected=%d%n",
            found, missing, unexpected);
        return true;
    }

    private static int candidateChunk(
            long seed, int regionX, int regionZ, boolean xAxis) {
        long regionSeed =
            seed
            + (long) regionX * REGION_X_MULTIPLIER
            + (long) regionZ * REGION_Z_MULTIPLIER
            + VILLAGE_SALT;
        Random random = new Random(regionSeed);
        int xOffset = random.nextInt(VILLAGE_RANGE);
        int zOffset = random.nextInt(VILLAGE_RANGE);
        return (xAxis ? regionX : regionZ) * VILLAGE_SPACING
            + (xAxis ? xOffset : zOffset);
    }

    private static void usage() {
        System.err.println(
            "Usage:\n"
            + "  VillageOracle1161 <seed> <startChunkX> <startChunkZ>\n"
            + "  VillageOracle1161 --scan <seed> <regionRadius>\n"
            + "  VillageOracle1161 --loot <seed> <startChunkX> "
            + "<startChunkZ> <jigsaw-manifest>");
    }

    public static void main(String[] args) {
        try {
            if (args.length == 3 && !"--scan".equals(args[0])) {
                long seed = Long.parseLong(args[0]);
                int chunkX = Integer.parseInt(args[1]);
                int chunkZ = Integer.parseInt(args[2]);
                SeedChecker checker = new SeedChecker(
                    seed, 1, SeedCheckerDimension.OVERWORLD);
                if (!printVillage(checker, seed, chunkX, chunkZ)) {
                    System.out.printf(
                        "N|%d|%d|%d%n", seed, chunkX, chunkZ);
                    System.exit(1);
                }
                return;
            }

            if (args.length == 3 && "--scan".equals(args[0])) {
                long seed = Long.parseLong(args[1]);
                int radius = Integer.parseInt(args[2]);
                if (radius < 0 || radius > 16)
                    throw new IllegalArgumentException(
                        "regionRadius must be between 0 and 16");

                SeedChecker checker = new SeedChecker(
                    seed, 1, SeedCheckerDimension.OVERWORLD);
                for (int regionZ = -radius;
                        regionZ <= radius; ++regionZ) {
                    for (int regionX = -radius;
                            regionX <= radius; ++regionX) {
                        int chunkX = candidateChunk(
                            seed, regionX, regionZ, true);
                        int chunkZ = candidateChunk(
                            seed, regionX, regionZ, false);
                        if (printVillage(
                                checker, seed, chunkX, chunkZ))
                            return;
                    }
                }
                System.out.printf(
                    "N|%d|radius=%d%n", seed, radius);
                System.exit(1);
                return;
            }

            if (args.length == 5 && "--loot".equals(args[0])) {
                long seed = Long.parseLong(args[1]);
                int chunkX = Integer.parseInt(args[2]);
                int chunkZ = Integer.parseInt(args[3]);
                Path manifest = Path.of(args[4]);
                if (!printVillageLoot(
                        seed, chunkX, chunkZ, manifest)) {
                    System.out.printf(
                        "N|%d|%d|%d%n",
                        seed, chunkX, chunkZ);
                    System.exit(1);
                }
                return;
            }
        } catch (NumberFormatException error) {
            System.err.println("Invalid integer: " + error.getMessage());
            System.exit(2);
            return;
        } catch (IOException error) {
            System.err.println(
                "Cannot read structure manifest: "
                + error.getMessage());
            System.exit(3);
            return;
        }

        usage();
        System.exit(2);
    }
}
