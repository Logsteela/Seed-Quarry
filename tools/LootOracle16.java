import com.seedfinding.mccore.util.data.Pair;
import com.seedfinding.mccore.version.MCVersion;
import com.seedfinding.mcfeature.loot.LootContext;
import com.seedfinding.mcfeature.loot.LootTable;
import com.seedfinding.mcfeature.loot.MCLootTables;
import com.seedfinding.mcfeature.loot.item.ItemStack;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Small console oracle for comparing the C port with the supplied MIT
 * SeedFinding Loot implementation. This does not start Minecraft or a GUI.
 */
public final class LootOracle16 {
    private LootOracle16() {}

    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println(
                "Usage: LootOracle16 <MCLootTables field> <LootTableSeed>");
            System.exit(2);
        }
        LootTable table = (LootTable) MCLootTables.class
            .getField(args[0]).get(null);
        long seed = Long.parseUnsignedLong(args[1]);
        List<String> output = new ArrayList<>();
        for (ItemStack stack : table.apply(MCVersion.v1_16_1).generate(
                new LootContext(seed, MCVersion.v1_16_1))) {
            StringBuilder line = new StringBuilder()
                .append(stack.getItem().getName())
                .append("=")
                .append(stack.getCount());
            for (Pair<String, Integer> enchantment
                    : stack.getItem().getEnchantments()) {
                line.append("@")
                    .append(enchantment.getFirst())
                    .append(":")
                    .append(enchantment.getSecond());
            }
            output.add(line.toString());
        }
        Collections.sort(output);
        for (String line : output)
            System.out.println(line);
    }
}
