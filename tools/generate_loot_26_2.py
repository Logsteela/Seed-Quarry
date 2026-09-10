#!/usr/bin/env python3
"""Extract compact 26.2 portal/Bastion facts from the official client JAR.

The JAR and generated jigsaw manifest stay local. Run again after changes to
this extractor; never silently apply a different release's tables or tags.
"""
import argparse
import json
import zipfile
from pathlib import Path

from extract_jigsaw_data import read_structure_nbt, extract_template, sha1_file
from generate_loot_tables_1_16_1 import integer_range, parse_entry
from generate_bastion_loot_tables_1_16_5 import render

SHA1 = "2dc72797acbc1b63fc16a11c4ac393605f453754"
TABLES = ["bastion_bridge", "bastion_hoglin_stable", "bastion_other",
          "bastion_treasure", "ruined_portal"]


def tag_values(archive, registry, value):
    if isinstance(value, list):
        result = []
        for child in value:
            for entry in tag_values(archive, registry, child):
                if entry not in result:
                    result.append(entry)
        return result
    if not value.startswith("#"):
        return [value]
    path = "data/minecraft/tags/{}/{}.json".format(registry, value[11:])
    return tag_values(archive, registry, json.loads(archive.read(path))["values"])


def generate(jar, output, manifest_path):
    if sha1_file(jar) != SHA1:
        raise ValueError("Expected the official Java 26.2 client JAR")
    tables = []
    enchantable = set()
    with zipfile.ZipFile(str(jar)) as archive:
        for name in TABLES:
            data = json.loads(archive.read(
                "data/minecraft/loot_table/chests/" + name + ".json"))
            pools = []
            for pool in data["pools"]:
                assert not pool.get("conditions") and not pool.get("functions")
                low, high = integer_range(pool["rolls"], name)
                entries = []
                for entry in pool["entries"]:
                    assert not entry.get("conditions") and not entry.get("quality")
                    for fn in entry.get("functions", []):
                        assert not fn.get("conditions")
                        if fn["function"] == "minecraft:enchant_randomly":
                            options = fn.pop("options")
                            if options == "minecraft:soul_speed":
                                fn["enchantments"] = [options]
                            elif options == "#minecraft:on_random_loot":
                                enchantable.add(entry["name"])
                            else:
                                raise ValueError("Unknown enchantment options: " + str(options))
                    entries.append(parse_entry(entry, name))
                pools.append({"min": low, "max": high, "entries": entries})
            tables.append({"name": name, "pools": pools})
        text = render(tables, SHA1).replace(
            "tools/generate_bastion_loot_tables_1_16_5.py", "tools/generate_loot_26_2.py"
        ).replace("1.16.5 client", "26.2 client").replace(
            "BASTION_LOOT_", "MODERN_LOOT_").replace("_1_16_5", "_26_2")
        text += "\nstatic const ModernEnchantment MODERN_ENCHANTMENTS_26_2[] = {\n"
        for item in sorted(enchantable):
            for enchantment in tag_values(archive, "enchantment", "#minecraft:on_random_loot"):
                data = json.loads(archive.read("data/minecraft/enchantment/" + enchantment[10:] + ".json"))
                if item not in tag_values(archive, "item", data["supported_items"]):
                    continue
                ench = enchantment[10:].upper().replace("SWEEPING_EDGE", "SWEEPING")
                text += "    {DP_LOOT_%s, DP_ENCH_%s, %d},\n" % (item[10:].upper(), ench, data["max_level"])
        text += "};\n\nstatic const uint8_t PORTAL_SIZES_26_2[][2] = {\n"
        prefix = "data/minecraft/structure/ruined_portal/"
        for name in ["portal_" + str(i) for i in range(1, 11)] + ["giant_portal_" + str(i) for i in range(1, 4)]:
            n = prefix + name + ".nbt"
            root = read_structure_nbt(archive.read(n), n)
            text += "    {%d, %d},\n" % (root["size"][0], root["size"][2])
        text += "};\n"
        prefix = "data/minecraft/structure/bastion/"
        templates = []
        for n in sorted(archive.namelist()):
            if not n.startswith(prefix) or not n.endswith(".nbt"):
                continue
            root = read_structure_nbt(archive.read(n), n)
            for block in root["blocks"]:
                nbt = block.get("nbt", {})
                assert not nbt.get("selection_priority") and not nbt.get("placement_priority")
            obj = extract_template(root, n, prefix)
            obj.pop("feature_blocks", None)
            templates.append(obj)
        pools = []
        prefix = "data/minecraft/worldgen/template_pool/"
        for n in sorted(archive.namelist()):
            if not n.startswith(prefix + "bastion/") or not n.endswith(".json"):
                continue
            data = json.loads(archive.read(n))
            elements = []
            for e in data["elements"]:
                el = e["element"]
                assert el["element_type"] == "minecraft:single_pool_element"
                assert el["projection"] == "rigid"
                elements.append({"template": el["location"][10:], "weight": e["weight"]})
            pools.append({"name": n[len(prefix):-5], "fallback": data["fallback"][10:],
                          "projection": "rigid", "elements": elements})
        assert len(templates) == 167 and len(pools) == 60
        assert sum(len(t.get("containers", [])) for t in templates) == 37
        manifest = {"format": 1, "minecraft_version": "26.2", "jar_sha1": SHA1,
                    "structures": {"bastion": templates},
                    "pools": {"bastion": {"definitions": pools}}}
    output.write_text(text, encoding="utf-8")
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, separators=(",", ":")), encoding="utf-8")
    print("Generated", output, "and", manifest_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("cubiomes/loot_tables_26_2.inc"))
    parser.add_argument("--manifest", type=Path, default=Path("build-structure-data/jigsaw-26.2.json"))
    args = parser.parse_args()
    generate(args.jar, args.output, args.manifest)
