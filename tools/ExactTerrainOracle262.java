import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintStream;

import net.minecraft.SharedConstants;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;

/**
 * Small line-oriented bridge to Minecraft 26.2's official terrain generator.
 *
 * Seed Quarry deliberately keeps portal Loot and template processing in C++.
 * This helper is only asked for the terrain-dependent anchor Y, after cheap
 * rejection has already happened. One daemon is shared by all search workers.
 */
public final class ExactTerrainOracle262 {
    private static final long MULTIPLIER = 0x5DEECE66DL;
    private static final long MASK = (1L << 48) - 1;

    // Must stay in sync with PortalLocation in portalcompletion16.cpp.
    private static final int ON_LAND = 0;
    private static final int ON_OCEAN_FLOOR = 1;
    private static final int UNDERGROUND = 2;
    private static final int IN_MOUNTAIN = 3;
    private static final int PARTLY_BURIED = 4;

    private final HolderLookup.Provider registries;
    private final NoiseBasedChunkGenerator normalGenerator;
    private final NoiseBasedChunkGenerator largeGenerator;
    private final LevelHeightAccessor heightAccessor;
    private long cachedSeed;
    private boolean cachedLarge;
    private boolean hasCachedSeed;
    private RandomState cachedRandomState;

    private ExactTerrainOracle262() {
        registries = VanillaRegistries.createLookup();
        var biomeParameters = registries.lookupOrThrow(
            Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST).getOrThrow(
                MultiNoiseBiomeSourceParameterLists.OVERWORLD);
        var source = MultiNoiseBiomeSource.createFromPreset(biomeParameters);
        normalGenerator = new NoiseBasedChunkGenerator(source,
            registries.lookupOrThrow(Registries.NOISE_SETTINGS).getOrThrow(
                NoiseGeneratorSettings.OVERWORLD));
        largeGenerator = new NoiseBasedChunkGenerator(source,
            registries.lookupOrThrow(Registries.NOISE_SETTINGS).getOrThrow(
                NoiseGeneratorSettings.LARGE_BIOMES));
        heightAccessor = LevelHeightAccessor.create(-64, 384);
    }

    private RandomState randomState(long seed, boolean large) {
        if (!hasCachedSeed || seed != cachedSeed || large != cachedLarge) {
            cachedRandomState = RandomState.create(
                registries, large ? NoiseGeneratorSettings.LARGE_BIOMES
                                  : NoiseGeneratorSettings.OVERWORLD,
                seed);
            cachedSeed = seed;
            cachedLarge = large;
            hasCachedSeed = true;
        }
        return cachedRandomState;
    }

    private static int nextBits(long[] state, int bits) {
        state[0] = (state[0] * MULTIPLIER + 11) & MASK;
        return (int)(state[0] >>> (48 - bits));
    }

    private static int nextInt(long[] state, int bound) {
        if (bound <= 0)
            throw new IllegalArgumentException("non-positive RNG bound");
        if ((bound & -bound) == bound)
            return (int)((bound * (long)nextBits(state, 31)) >> 31);
        int bits;
        int value;
        do {
            bits = nextBits(state, 31);
            value = bits % bound;
        } while (bits - value + (bound - 1) < 0);
        return value;
    }

    private static int nextInclusive(long[] state, int minimum, int maximum) {
        if (minimum >= maximum)
            return maximum;
        return minimum + nextInt(state, maximum - minimum + 1);
    }

    private int portalY(String[] fields) {
        if (fields.length != 11)
            throw new IllegalArgumentException("P expects 10 arguments");

        long seed = Long.parseUnsignedLong(fields[1]);
        long[] structureRandom = {Long.parseUnsignedLong(fields[2]) & MASK};
        int location = Integer.parseInt(fields[3]);
        int ySpan = Integer.parseInt(fields[4]);
        int minX = Integer.parseInt(fields[5]);
        int minZ = Integer.parseInt(fields[6]);
        int maxX = Integer.parseInt(fields[7]);
        int maxZ = Integer.parseInt(fields[8]);
        boolean large = Integer.parseInt(fields[9]) != 0;
        int expectedProtocol = Integer.parseInt(fields[10]);
        if (expectedProtocol != 262)
            throw new IllegalArgumentException("unsupported terrain protocol");
        if (location < ON_LAND || location > PARTLY_BURIED ||
            minX > maxX || minZ > maxZ || ySpan <= 0)
            throw new IllegalArgumentException("invalid portal geometry");

        NoiseBasedChunkGenerator generator = large
            ? largeGenerator : normalGenerator;
        RandomState state = randomState(seed, large);
        Heightmap.Types heightmap = location == ON_OCEAN_FLOOR
            ? Heightmap.Types.OCEAN_FLOOR_WG
            : Heightmap.Types.WORLD_SURFACE_WG;
        int centerX = minX + (maxX - minX + 1) / 2;
        int centerZ = minZ + (maxZ - minZ + 1) / 2;
        int surfaceY = generator.getBaseHeight(
            centerX, centerZ, heightmap, heightAccessor, state) - 1;

        final int minimumY = heightAccessor.getMinY() + 15;
        int projectedY;
        if (location == IN_MOUNTAIN) {
            projectedY = nextInclusive(
                structureRandom, 70, surfaceY - ySpan);
        } else if (location == UNDERGROUND) {
            projectedY = nextInclusive(
                structureRandom, minimumY, surfaceY - ySpan);
        } else if (location == PARTLY_BURIED) {
            projectedY = surfaceY - ySpan +
                nextInclusive(structureRandom, 2, 8);
        } else {
            projectedY = surfaceY;
        }

        NoiseColumn[] columns = {
            generator.getBaseColumn(minX, minZ, heightAccessor, state),
            generator.getBaseColumn(maxX, minZ, heightAccessor, state),
            generator.getBaseColumn(minX, maxZ, heightAccessor, state),
            generator.getBaseColumn(maxX, maxZ, heightAccessor, state),
        };
        for (; projectedY > minimumY; --projectedY) {
            int solid = 0;
            for (NoiseColumn column : columns) {
                if (heightmap.isOpaque().test(column.getBlock(projectedY)) &&
                    ++solid == 3)
                    return projectedY;
            }
        }
        return projectedY;
    }

    private void serve(BufferedReader input, PrintStream output)
        throws Exception {
        output.println("SQREADY 262");
        output.flush();
        String line;
        while ((line = input.readLine()) != null) {
            line = line.trim();
            if (line.equals("Q"))
                return;
            try {
                String[] fields = line.split("\\s+");
                if (fields.length == 0 || !fields[0].equals("P"))
                    throw new IllegalArgumentException("unknown command");
                output.println("SQOK " + portalY(fields));
            } catch (Throwable error) {
                String message = error.getMessage();
                if (message == null || message.isBlank())
                    message = error.getClass().getSimpleName();
                output.println("SQERR " + message.replace('\n', ' '));
            }
            output.flush();
        }
    }

    public static void main(String[] args) throws Exception {
        // Bootstrap installs a logging stdout wrapper. Keep a private protocol
        // stream so log messages can never be mistaken for daemon replies.
        PrintStream protocol = System.out;
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        System.setOut(protocol);
        ExactTerrainOracle262 oracle = new ExactTerrainOracle262();
        oracle.serve(new BufferedReader(new InputStreamReader(System.in)),
            protocol);
    }
}
