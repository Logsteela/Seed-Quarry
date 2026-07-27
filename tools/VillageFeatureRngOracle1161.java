import java.util.HashSet;
import java.util.Random;
import java.util.Set;

/**
 * Small java.util.Random-only oracle for the flat, structure-free branches
 * of the Java 1.16.1 Village FeaturePoolElement algorithms.
 */
public final class VillageFeatureRngOracle1161 {
    private static Random villageRandom(long worldSeed) {
        Random random = new Random(worldSeed);
        random.nextLong();
        random.nextLong();
        random.setSeed(worldSeed + 40011L);
        return random;
    }

    private static long finish(Random random) {
        random.nextLong(); // container without a LootTable
        return random.nextLong();
    }

    private static void normalTree(Random random) {
        random.nextInt(3);
        random.nextInt(1);
        random.nextInt(1);
        random.nextInt(1);
        for (int corner = 0; corner < 16; ++corner)
            random.nextInt(2);
    }

    private static void pineTree(Random random) {
        int height = 6 + random.nextInt(5) + random.nextInt(1);
        int foliageHeight = 3 + random.nextInt(2);
        random.nextInt(1);
        random.nextInt(height - foliageHeight + 1);
        random.nextInt(1);
    }

    private static void spruceTree(Random random) {
        int height = 5 + random.nextInt(3) + random.nextInt(2);
        Math.max(4, height - 1 - random.nextInt(2));
        random.nextInt(2);
        random.nextInt(3);
        random.nextInt(2);
    }

    private static int[] direction(int index) {
        return new int[][] {
            {0, -1}, {1, 0}, {0, 1}, {-1, 0}
        }[index];
    }

    private static void acaciaTree(Random random) {
        int height = 5 + random.nextInt(3) + random.nextInt(3);
        random.nextInt(1);
        int first = random.nextInt(4);
        int bendStart = height - random.nextInt(4) - 1;
        int bendLength = 3 - random.nextInt(3);
        int x = 0;
        int z = 0;
        Set<String> logs = new HashSet<>();
        for (int y = 0; y < height; ++y) {
            if (y >= bendStart && bendLength > 0) {
                int[] step = direction(first);
                x += step[0];
                z += step[1];
                --bendLength;
            }
            logs.add(x + ":" + y + ":" + z);
        }

        int attachments = 1;
        x = 0;
        z = 0;
        int second = random.nextInt(4);
        if (second != first) {
            int branchStart = bendStart - random.nextInt(2) - 1;
            int branchLength = 1 + random.nextInt(3);
            boolean placed = false;
            for (int y = branchStart;
                    y < height && branchLength > 0;
                    ++y, --branchLength) {
                if (y < 1)
                    continue;
                int[] step = direction(second);
                x += step[0];
                z += step[1];
                placed |= logs.add(x + ":" + y + ":" + z);
            }
            if (placed)
                ++attachments;
        }
        for (int i = 0; i < attachments; ++i)
            random.nextInt(1);
    }

    private static void blockPile(Random random, int provider) {
        int radiusX = 2 + random.nextInt(2);
        int radiusZ = 2 + random.nextInt(2);
        Set<String> placed = new HashSet<>();
        for (int z = -radiusZ; z <= radiusZ; ++z) {
            for (int y = 0; y <= 1; ++y) {
                for (int x = -radiusX; x <= radiusX; ++x) {
                    float threshold = random.nextFloat() * 10.0F
                        - random.nextFloat() * 6.0F;
                    boolean tries = x * x + z * z <= threshold;
                    if (!tries)
                        tries = random.nextFloat() < 0.031;
                    if (!tries)
                        continue;
                    String here = x + ":" + y + ":" + z;
                    if (placed.contains(here))
                        continue;
                    boolean supported = y == 0
                        || placed.contains(x + ":0:" + z);
                    if (!supported)
                        continue;
                    placed.add(here);
                    if (provider == 1)
                        random.nextInt(3);
                    else if (provider == 2) {
                        random.nextFloat();
                        random.nextFloat();
                    }
                }
            }
        }
    }

    private static void cactus(Random random) {
        Set<String> cactus = new HashSet<>();
        for (int attempt = 0; attempt < 10; ++attempt) {
            int x = random.nextInt(8) - random.nextInt(8);
            int y = 64 + random.nextInt(4) - random.nextInt(4);
            int z = random.nextInt(8) - random.nextInt(8);
            if (y != 64 || cactus.contains(x + ":" + y + ":" + z))
                continue;
            boolean adjacent = false;
            int[][] offsets = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (int[] offset : offsets) {
                if (cactus.contains(
                        (x + offset[0]) + ":" + y + ":"
                        + (z + offset[1]))) {
                    adjacent = true;
                    break;
                }
            }
            if (adjacent)
                continue;
            int extra = random.nextInt(3);
            int height = 1 + random.nextInt(extra + 1);
            for (int dy = 0; dy < height; ++dy)
                cactus.add(x + ":" + (y + dy) + ":" + z);
        }
    }

    private static void print(String name, long worldSeed, int kind) {
        Random random = villageRandom(worldSeed);
        switch (kind) {
        case 0: normalTree(random); break;
        case 1: pineTree(random); break;
        case 2: spruceTree(random); break;
        case 3: acaciaTree(random); break;
        case 4: blockPile(random, 1); break;
        case 5: blockPile(random, 2); break;
        case 6: cactus(random); break;
        default: throw new AssertionError(kind);
        }
        System.out.println(name + "=" + finish(random));
    }

    public static void main(String[] args) {
        long seed = args.length == 0 ? 1L : Long.parseLong(args[0]);
        print("normal", seed, 0);
        print("pine", seed, 1);
        print("spruce", seed, 2);
        print("acacia", seed, 3);
        print("hay", seed, 4);
        print("weighted_pile", seed, 5);
        print("cactus", seed, 6);
    }
}
