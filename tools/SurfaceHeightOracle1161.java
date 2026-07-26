import nl.jellejurre.seedchecker.SeedCheckerSettings;

import net.minecraft.world.Heightmap;
import net.minecraft.world.gen.GeneratorOptions;
import net.minecraft.world.gen.chunk.SurfaceChunkGenerator;

/**
 * Development-only oracle for Java 1.16.1 WORLD_SURFACE_WG heights.
 *
 * The published SeedChecker jar supplies the mapped Minecraft implementation;
 * this source only calls its public chunk-generator API.
 */
public final class SurfaceHeightOracle1161 {
    private SurfaceHeightOracle1161() {}

    private static void usage() {
        System.err.println(
            "Usage: SurfaceHeightOracle1161 <seed> <x> <z> [<x> <z> ...]");
    }

    public static void main(String[] args) {
        if (args.length < 3 || (args.length & 1) == 0) {
            usage();
            System.exit(2);
        }

        try {
            long seed = Long.parseLong(args[0]);
            SeedCheckerSettings.initialise();
            SurfaceChunkGenerator generator =
                GeneratorOptions.createOverworldGenerator(seed);
            for (int index = 1; index < args.length; index += 2) {
                int x = Integer.parseInt(args[index]);
                int z = Integer.parseInt(args[index + 1]);
                int height = generator.getHeightOnGround(
                    x, z, Heightmap.Type.WORLD_SURFACE_WG);
                System.out.printf(
                    "H|%d|%d|%d|%d%n", seed, x, z, height);
            }
        } catch (NumberFormatException error) {
            System.err.println("Invalid integer: " + error.getMessage());
            System.exit(2);
        }
    }
}
