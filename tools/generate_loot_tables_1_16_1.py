#!/usr/bin/env python3
"""Generate compact C metadata for the 1.16.1 village/Bastion Loot tables.

The official Minecraft jar remains outside the repository.  The generated
file contains only table ids, item ids, weights, integer ranges, and function
flags needed by the clean-room Loot evaluator.
"""

import argparse
import hashlib
import json
import os
import sys
import zipfile
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple


OFFICIAL_CLIENT_SHA1 = "c9abbe8ee4fa490751ca70635340b7cf00db83ff"

TABLES: List[Tuple[str, str]] = [
    ("village/village_armorer", "VILLAGE_ARMORER"),
    ("village/village_butcher", "VILLAGE_BUTCHER"),
    ("village/village_cartographer", "VILLAGE_CARTOGRAPHER"),
    ("village/village_desert_house", "VILLAGE_DESERT_HOUSE"),
    ("village/village_fisher", "VILLAGE_FISHER"),
    ("village/village_fletcher", "VILLAGE_FLETCHER"),
    ("village/village_mason", "VILLAGE_MASON"),
    ("village/village_plains_house", "VILLAGE_PLAINS_HOUSE"),
    ("village/village_savanna_house", "VILLAGE_SAVANNA_HOUSE"),
    ("village/village_shepherd", "VILLAGE_SHEPHERD"),
    ("village/village_snowy_house", "VILLAGE_SNOWY_HOUSE"),
    ("village/village_taiga_house", "VILLAGE_TAIGA_HOUSE"),
    ("village/village_tannery", "VILLAGE_TANNERY"),
    ("village/village_temple", "VILLAGE_TEMPLE"),
    ("village/village_toolsmith", "VILLAGE_TOOLSMITH"),
    ("village/village_weaponsmith", "VILLAGE_WEAPONSMITH"),
    ("bastion_bridge", "BASTION_BRIDGE"),
    ("bastion_hoglin_stable", "BASTION_HOGLIN_STABLE"),
    ("bastion_other", "BASTION_OTHER"),
    ("bastion_treasure", "BASTION_TREASURE"),
]


def default_jar() -> Path:
    appdata = os.environ.get("APPDATA")
    if not appdata:
        raise RuntimeError("APPDATA is not set; pass --jar explicitly")
    return (
        Path(appdata)
        / ".minecraft"
        / "versions"
        / "1.16.1"
        / "1.16.1.jar"
    )


def sha1_file(path: Path) -> str:
    digest = hashlib.sha1()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def integer_range(value: Any, context: str) -> Tuple[int, int]:
    if isinstance(value, (int, float)):
        result = int(value)
        if float(value) != result:
            raise ValueError(f"{context}: non-integral value {value!r}")
        return result, result
    if not isinstance(value, dict) or value.get("type") != "minecraft:uniform":
        raise ValueError(f"{context}: unsupported range {value!r}")
    low = int(value["min"])
    high = int(value["max"])
    if float(value["min"]) != low or float(value["max"]) != high:
        raise ValueError(f"{context}: non-integral uniform range {value!r}")
    return low, high


def item_symbol(name: str, fixed_soul_speed: bool) -> str:
    if not name.startswith("minecraft:"):
        raise ValueError(f"unsupported item namespace: {name}")
    item = name[len("minecraft:"):]
    # Vanilla converts a book to an enchanted book when enchant_randomly runs.
    if item == "book" and fixed_soul_speed:
        item = "enchanted_book"
    return "DP_LOOT_" + item.upper()


def parse_entry(entry: Dict[str, Any], context: str) -> Dict[str, Any]:
    entry_type = entry.get("type")
    if entry_type == "minecraft:empty":
        if entry.get("functions"):
            raise ValueError(f"{context}: empty entry has functions")
        return {
            "item": "-1",
            "weight": int(entry.get("weight", 1)),
            "min": 0,
            "max": 0,
            "flags": ["LT16_EMPTY"],
        }
    if entry_type != "minecraft:item":
        raise ValueError(f"{context}: unsupported entry type {entry_type!r}")

    low = high = 1
    flags: List[str] = []
    fixed_soul_speed = False
    for function in entry.get("functions", []):
        function_type = function.get("function")
        if function_type == "minecraft:set_count":
            low, high = integer_range(
                function["count"], context + " set_count"
            )
        elif function_type == "minecraft:set_damage":
            # The exact damage value is irrelevant to item-count filters, but
            # Vanilla consumes one nextFloat call.
            flags.append("LT16_DAMAGE")
        elif function_type == "minecraft:enchant_randomly":
            flags.append("LT16_ENCHANT")
            enchantments = function.get("enchantments")
            if enchantments is not None:
                if enchantments != ["minecraft:soul_speed"]:
                    raise ValueError(
                        f"{context}: unsupported fixed enchantments "
                        f"{enchantments!r}"
                    )
                fixed_soul_speed = True
                flags.append("LT16_SOUL_SPEED")
        else:
            raise ValueError(
                f"{context}: unsupported Loot function {function_type!r}"
            )

    return {
        "item": item_symbol(entry["name"], fixed_soul_speed),
        "weight": int(entry.get("weight", 1)),
        "min": low,
        "max": high,
        "flags": flags or ["LT16_NONE"],
    }


def read_tables(jar: Path) -> Tuple[List[Dict[str, Any]], str]:
    digest = sha1_file(jar)
    if digest != OFFICIAL_CLIENT_SHA1:
        raise ValueError(
            "Expected the official 1.16.1 client jar SHA-1 "
            f"{OFFICIAL_CLIENT_SHA1}, received {digest}"
        )
    parsed: List[Dict[str, Any]] = []
    with zipfile.ZipFile(str(jar), "r") as archive:
        for path, enum_suffix in TABLES:
            entry_name = (
                "data/minecraft/loot_tables/chests/" + path + ".json"
            )
            data = json.loads(archive.read(entry_name))
            pools: List[Dict[str, Any]] = []
            for pool_index, pool in enumerate(data.get("pools", [])):
                context = f"{entry_name} pool {pool_index}"
                if pool.get("conditions") or pool.get("functions"):
                    raise ValueError(
                        f"{context}: pool conditions/functions unsupported"
                    )
                low, high = integer_range(pool["rolls"], context + " rolls")
                entries = [
                    parse_entry(entry, f"{context} entry {entry_index}")
                    for entry_index, entry in enumerate(pool["entries"])
                ]
                pools.append({
                    "min": low,
                    "max": high,
                    "entries": entries,
                })
            parsed.append({
                "path": path,
                "enum": enum_suffix,
                "pools": pools,
            })
    return parsed, digest


def joined_flags(flags: Iterable[str]) -> str:
    return " | ".join(flags)


def render(tables: List[Dict[str, Any]], digest: str) -> str:
    entries: List[Dict[str, Any]] = []
    pools: List[Dict[str, Any]] = []
    rendered_tables: List[Tuple[int, int]] = []
    for table in tables:
        first_pool = len(pools)
        for pool in table["pools"]:
            first_entry = len(entries)
            entries.extend(pool["entries"])
            pools.append({
                "min": pool["min"],
                "max": pool["max"],
                "first": first_entry,
                "count": len(pool["entries"]),
            })
        rendered_tables.append((first_pool, len(table["pools"])))

    lines = [
        "/* Generated by tools/generate_loot_tables_1_16_1.py.",
        f" * Official 1.16.1 client jar SHA-1: {digest}",
        " * Contains compact Loot metadata only; no raw JSON is embedded.",
        " */",
        "",
        "static const LootTableEntry16 STRUCTURE_LOOT_ENTRIES_16[] = {",
    ]
    for entry in entries:
        lines.append(
            "    {%s, %d, %d, %d, %s},"
            % (
                entry["item"],
                entry["weight"],
                entry["min"],
                entry["max"],
                joined_flags(entry["flags"]),
            )
        )
    lines += [
        "};",
        "",
        "static const LootTablePool16 STRUCTURE_LOOT_POOLS_16[] = {",
    ]
    for pool in pools:
        lines.append(
            "    {%d, %d, %d, %d},"
            % (pool["min"], pool["max"], pool["first"], pool["count"])
        )
    lines += [
        "};",
        "",
        "static const LootTableDefinition16 STRUCTURE_LOOT_TABLES_16[] = {",
    ]
    for first, count in rendered_tables:
        lines.append(f"    {{{first}, {count}}},")
    lines += [
        "};",
        "",
        "static const char *const STRUCTURE_LOOT_TABLE_NAMES_16[] = {",
    ]
    for table in tables:
        lines.append(f'    "{table["path"]}",')
    lines += [
        "};",
        "",
    ]
    return "\n".join(lines)


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("cubiomes") / "loot_tables_1_16_1.inc",
    )
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    jar = args.jar or default_jar()
    try:
        tables, digest = read_tables(jar)
        output = render(tables, digest)
    except (
        KeyError,
        OSError,
        RuntimeError,
        ValueError,
        zipfile.BadZipFile,
    ) as error:
        print(f"Loot table generation failed: {error}", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(output)
    print(
        f"Wrote {args.output}: {len(tables)} tables, "
        f"{sum(len(t['pools']) for t in tables)} pools"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
