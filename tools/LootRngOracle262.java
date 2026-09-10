import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;

/** Tiny console-only check against the official 26.2 JAR; no world is opened. */
public final class LootRngOracle262 {
    public static void main(String[] args) {
        long[] seeds = {0L, 1L, 281474976710657L, -1L};
        int[][] chunks = {{0, 0}, {-7, 19}, {12, -34}};
        for (long seed : seeds) for (int[] chunk : chunks) {
            var random = new WorldgenRandom(new XoroshiroRandomSource(0L));
            long population = random.setDecorationSeed(seed, chunk[0] * 16, chunk[1] * 16);
            random.setFeatureSeed(population, 0, 4);
            System.out.printf("%016x %d %d %016x %016x %016x%n",
                seed, chunk[0], chunk[1], population, random.nextLong(), random.nextLong());
        }
    }
}
