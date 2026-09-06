import nl.jellejurre.seedchecker.SeedChecker;
import nl.jellejurre.seedchecker.SeedCheckerDimension;

import net.minecraft.structure.PoolStructurePiece;
import net.minecraft.structure.StructurePiece;
import net.minecraft.structure.StructureStart;
import net.minecraft.structure.pool.FeaturePoolElement;
import net.minecraft.world.chunk.ProtoChunk;
import net.minecraft.world.gen.feature.DefaultBiomeFeatures;
import net.minecraft.world.gen.feature.ConfiguredFeature;
import net.minecraft.world.gen.feature.StructureFeature;
import net.minecraft.world.gen.feature.TreeFeatureConfig;

import java.lang.reflect.Field;

/** Prints the height rules used by the four Village tree configs. */
public final class VillageFeatureConfigProbe1161 {
    private static void print(String name, TreeFeatureConfig config) {
        System.out.printf(
            "%s|max_water_depth=%d|heightmap=%s|skip_fluid=%s%n",
            name, config.maxWaterDepth, config.heightmap,
            config.skipFluidCheck);
    }

    public static void main(String[] args) {
        if (args.length != 3)
            throw new IllegalArgumentException("seed chunkX chunkZ");
        long seed = Long.parseLong(args[0]);
        int chunkX = Integer.parseInt(args[1]);
        int chunkZ = Integer.parseInt(args[2]);
        SeedChecker checker = new SeedChecker(
            seed, 1, SeedCheckerDimension.OVERWORLD);
        print("normal", DefaultBiomeFeatures.OAK_TREE_CONFIG);
        print("pine", DefaultBiomeFeatures.PINE_TREE_CONFIG);
        print("spruce", DefaultBiomeFeatures.SPRUCE_TREE_CONFIG);
        print("acacia", DefaultBiomeFeatures.ACACIA_TREE_CONFIG);
        ProtoChunk chunk = checker.getChunk(chunkX, chunkZ, 1);
        StructureStart<?> start = chunk.getStructureStart(
            StructureFeature.VILLAGE);
        try {
            Field featureField = FeaturePoolElement.class
                .getDeclaredField("feature");
            featureField.setAccessible(true);
            int index = 0;
            for (StructurePiece raw : start.getChildren()) {
                if (raw instanceof PoolStructurePiece) {
                    Object element = ((PoolStructurePiece) raw)
                        .getPoolElement();
                    if (element instanceof FeaturePoolElement) {
                        ConfiguredFeature<?, ?> configured =
                            (ConfiguredFeature<?, ?>) featureField.get(element);
                        if (configured.config instanceof TreeFeatureConfig) {
                            System.out.printf("piece=%d|", index);
                            print("runtime",
                                (TreeFeatureConfig) configured.config);
                        }
                    }
                }
                ++index;
            }
        } catch (ReflectiveOperationException error) {
            throw new RuntimeException(error);
        }
    }
}
