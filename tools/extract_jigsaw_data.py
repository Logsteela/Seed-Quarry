#!/usr/bin/env python3
"""Extract deterministic structure facts from an official Minecraft jar.

This tool records only the facts needed by Seed Atlas' jigsaw and compact
Village feature simulation:

* template dimensions;
* jigsaw connector positions and attributes;
* randomizable-container positions, block types, and LootTable ids.
* grass paths and coarse block-material categories near feature points.

Minecraft 1.16 structure NBT files are gzip-compressed inside the jar. The
parser below intentionally supports the complete standard NBT tag set so the
extractor has no third-party Python dependency.
"""

import argparse
import gzip
import hashlib
import io
import json
import os
import re
import struct
import sys
import zipfile
from pathlib import Path
from typing import Any, BinaryIO, Dict, List, Tuple


TAG_END = 0
TAG_BYTE = 1
TAG_SHORT = 2
TAG_INT = 3
TAG_LONG = 4
TAG_FLOAT = 5
TAG_DOUBLE = 6
TAG_BYTE_ARRAY = 7
TAG_STRING = 8
TAG_LIST = 9
TAG_COMPOUND = 10
TAG_INT_ARRAY = 11
TAG_LONG_ARRAY = 12

STRUCTURE_PREFIXES = {
    "village": "data/minecraft/structures/village/",
    "bastion": "data/minecraft/structures/bastion/",
}

BASTION_POOL_FILES = (
    "BastionBridgePools.java",
    "BastionHoglinStablePools.java",
    "BastionHousingUnitsPools.java",
    "BastionSharedPools.java",
    "BastionTreasureRoomPools.java",
)

VILLAGE_POOL_FILES = (
    "DesertVillagePools.java",
    "PlainVillagePools.java",
    "SavannaVillagePools.java",
    "SnowyVillagePools.java",
    "TaigaVillagePools.java",
)

# StructureTemplate assigns a fresh LootTableSeed to every placed
# RandomizableContainerBlockEntity, including containers with no LootTable.
RANDOMIZABLE_CONTAINERS = {
    "minecraft:barrel",
    "minecraft:chest",
    "minecraft:dispenser",
    "minecraft:dropper",
    "minecraft:trapped_chest",
}

TREE_FREE_BLOCKS = {
    "minecraft:air",
    "minecraft:cave_air",
    "minecraft:void_air",
    "minecraft:water",
    "minecraft:grass",
    "minecraft:fern",
    "minecraft:tall_grass",
    "minecraft:large_fern",
    "minecraft:snow",
    "minecraft:wheat",
    "minecraft:carrots",
    "minecraft:potatoes",
    "minecraft:beetroots",
    "minecraft:pumpkin_stem",
    "minecraft:melon_stem",
    "minecraft:sugar_cane",
    "minecraft:dead_bush",
    "minecraft:dandelion",
    "minecraft:poppy",
    "minecraft:blue_orchid",
    "minecraft:allium",
    "minecraft:azure_bluet",
    "minecraft:oxeye_daisy",
    "minecraft:cornflower",
    "minecraft:lily_of_the_valley",
}

NON_STURDY_SUFFIXES = (
    "_stairs", "_slab", "_fence", "_wall", "_door", "_trapdoor",
    "_pane", "_torch", "_lantern", "_carpet", "_bed", "_button",
    "_pressure_plate", "_sapling", "_flower", "_tulip",
)


def feature_block_kind(block_name: str) -> str:
    if block_name in (
        "minecraft:dirt",
        "minecraft:grass_block",
        "minecraft:podzol",
        "minecraft:coarse_dirt",
        "minecraft:mycelium",
        "minecraft:farmland",
    ):
        return "soil"
    if block_name in ("minecraft:sand", "minecraft:red_sand"):
        return "sand"
    if block_name == "minecraft:water":
        return "water"
    if (
        block_name in TREE_FREE_BLOCKS
    ):
        return "tree_free"
    if block_name.endswith("_leaves"):
        return "tree_free_solid"
    if (
        block_name.endswith("_log")
        or block_name.endswith("_wood")
    ):
        return "tree_free_sturdy"
    if block_name.endswith(NON_STURDY_SUFFIXES) or block_name in {
        "minecraft:bell",
        "minecraft:campfire",
        "minecraft:grindstone",
        "minecraft:lectern",
        "minecraft:brewing_stand",
        "minecraft:cauldron",
        "minecraft:composter",
        "minecraft:chest",
        "minecraft:barrel",
        "minecraft:iron_bars",
        "minecraft:cobweb",
        "minecraft:ladder",
        "minecraft:vine",
    }:
        return "occupied"
    return "sturdy"


class NbtError(ValueError):
    """Raised when an NBT payload is malformed or unsupported."""


class NbtReader:
    def __init__(self, stream: BinaryIO) -> None:
        self.stream = stream

    def read_exact(self, size: int) -> bytes:
        data = self.stream.read(size)
        if len(data) != size:
            raise NbtError(
                "Unexpected end of NBT data: "
                f"wanted {size} bytes, received {len(data)}"
            )
        return data

    def unpack(self, fmt: str) -> Any:
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.read_exact(size))[0]

    def read_string(self) -> str:
        size = self.unpack(">H")
        return self.read_exact(size).decode("utf-8")

    def read_payload(self, tag_type: int) -> Any:
        if tag_type == TAG_BYTE:
            return self.unpack(">b")
        if tag_type == TAG_SHORT:
            return self.unpack(">h")
        if tag_type == TAG_INT:
            return self.unpack(">i")
        if tag_type == TAG_LONG:
            return self.unpack(">q")
        if tag_type == TAG_FLOAT:
            return self.unpack(">f")
        if tag_type == TAG_DOUBLE:
            return self.unpack(">d")
        if tag_type == TAG_BYTE_ARRAY:
            size = self.unpack(">i")
            if size < 0:
                raise NbtError(f"Negative byte-array size: {size}")
            return list(self.read_exact(size))
        if tag_type == TAG_STRING:
            return self.read_string()
        if tag_type == TAG_LIST:
            item_type = self.unpack(">B")
            size = self.unpack(">i")
            if size < 0:
                raise NbtError(f"Negative list size: {size}")
            return [self.read_payload(item_type) for _ in range(size)]
        if tag_type == TAG_COMPOUND:
            result: Dict[str, Any] = {}
            while True:
                child_type = self.unpack(">B")
                if child_type == TAG_END:
                    return result
                child_name = self.read_string()
                result[child_name] = self.read_payload(child_type)
        if tag_type == TAG_INT_ARRAY:
            size = self.unpack(">i")
            if size < 0:
                raise NbtError(f"Negative int-array size: {size}")
            return [self.unpack(">i") for _ in range(size)]
        if tag_type == TAG_LONG_ARRAY:
            size = self.unpack(">i")
            if size < 0:
                raise NbtError(f"Negative long-array size: {size}")
            return [self.unpack(">q") for _ in range(size)]
        raise NbtError(f"Unsupported NBT tag type: {tag_type}")

    def read_root(self) -> Dict[str, Any]:
        tag_type = self.unpack(">B")
        if tag_type != TAG_COMPOUND:
            raise NbtError(
                f"NBT root must be a compound, received tag {tag_type}"
            )
        self.read_string()  # Root name; structure files normally use "".
        root = self.read_payload(TAG_COMPOUND)
        if not isinstance(root, dict):
            raise NbtError("NBT compound decoder returned an invalid root")
        return root


def strip_namespace(value: str) -> str:
    prefix = "minecraft:"
    return value[len(prefix):] if value.startswith(prefix) else value


def official_jar_path(version: str) -> Path:
    appdata = os.environ.get("APPDATA")
    if not appdata:
        raise FileNotFoundError(
            "APPDATA is not set; specify the official jar with --jar"
        )
    return Path(appdata) / ".minecraft" / "versions" / version / (
        version + ".jar"
    )


def read_structure_nbt(payload: bytes, entry_name: str) -> Dict[str, Any]:
    try:
        decoded = gzip.decompress(payload)
    except OSError as error:
        raise NbtError(f"{entry_name}: invalid gzip payload: {error}") from error
    try:
        return NbtReader(io.BytesIO(decoded)).read_root()
    except (NbtError, UnicodeDecodeError, struct.error) as error:
        raise NbtError(f"{entry_name}: {error}") from error


def palette_from_root(root: Dict[str, Any], entry_name: str) -> List[Any]:
    if "palette" in root:
        palette = root["palette"]
    elif "palettes" in root and root["palettes"]:
        # Multiple palettes only vary block states. Jigsaw/container positions
        # and their block-entity NBT remain in the shared blocks list.
        palette = root["palettes"][0]
    else:
        raise NbtError(f"{entry_name}: missing palette/palettes")
    if not isinstance(palette, list):
        raise NbtError(f"{entry_name}: palette is not a list")
    return palette


def vector3(value: Any, field: str, entry_name: str) -> List[int]:
    if (
        not isinstance(value, list)
        or len(value) != 3
        or any(not isinstance(component, int) for component in value)
    ):
        raise NbtError(f"{entry_name}: {field} is not an integer vector3")
    return value


def template_name(entry_name: str, prefix: str) -> str:
    return entry_name[len(prefix):-len(".nbt")]


def extract_template(
    root: Dict[str, Any], entry_name: str, prefix: str
) -> Dict[str, Any]:
    palette = palette_from_root(root, entry_name)
    size = vector3(root.get("size"), "size", entry_name)
    blocks = root.get("blocks")
    if not isinstance(blocks, list):
        raise NbtError(f"{entry_name}: blocks is not a list")

    jigsaws: List[Dict[str, Any]] = []
    containers: List[Dict[str, Any]] = []
    grass_path_positions: List[List[int]] = []
    occupied_positions = set()
    feature_blocks: List[Dict[str, Any]] = []
    for placement_index, block in enumerate(blocks):
        if not isinstance(block, dict):
            raise NbtError(f"{entry_name}: malformed block entry")
        state_index = block.get("state")
        if not isinstance(state_index, int) or not 0 <= state_index < len(
            palette
        ):
            raise NbtError(
                f"{entry_name}: palette index out of range: {state_index}"
            )
        state = palette[state_index]
        if not isinstance(state, dict):
            raise NbtError(f"{entry_name}: malformed palette entry")
        block_name = state.get("Name")
        if not isinstance(block_name, str):
            raise NbtError(f"{entry_name}: palette block has no Name")
        pos = vector3(block.get("pos"), "block.pos", entry_name)
        nbt = block.get("nbt")

        if block_name not in (
            "minecraft:air",
            "minecraft:cave_air",
            "minecraft:void_air",
            "minecraft:structure_block",
        ):
            occupied_positions.add(tuple(pos))

        placed_name = block_name

        if block_name == "minecraft:jigsaw":
            if not isinstance(nbt, dict):
                raise NbtError(f"{entry_name}: jigsaw block has no NBT")
            properties = state.get("Properties", {})
            orientation = properties.get("orientation")
            if not isinstance(orientation, str):
                raise NbtError(
                    f"{entry_name}: jigsaw block has no orientation"
                )
            front = orientation.split("_", 1)[0]
            default_joint = (
                "rollable" if front in ("up", "down") else "aligned"
            )
            jigsaws.append({
                "pos": pos,
                "orientation": orientation,
                "name": strip_namespace(str(nbt.get("name", "empty"))),
                "target": strip_namespace(str(nbt.get("target", "empty"))),
                "pool": strip_namespace(str(nbt.get("pool", "empty"))),
                "joint": str(nbt.get("joint", default_joint)),
                "final_state": strip_namespace(
                    str(nbt.get("final_state", "air"))
                ),
                "placement_index": placement_index,
            })
            occupied_positions.discard(tuple(pos))
            final_name = str(nbt.get("final_state", "")).split("[", 1)[0]
            placed_name = final_name
            if final_name not in (
                "minecraft:air",
                "minecraft:cave_air",
                "minecraft:void_air",
                "minecraft:structure_void",
            ):
                occupied_positions.add(tuple(pos))
            if final_name == "minecraft:grass_path":
                grass_path_positions.append(pos)

        if block_name == "minecraft:grass_path":
            grass_path_positions.append(pos)

        if placed_name not in (
            "minecraft:air",
            "minecraft:cave_air",
            "minecraft:void_air",
            "minecraft:structure_air",
            "minecraft:structure_block",
            "minecraft:structure_void",
        ):
            feature_blocks.append({
                "pos": pos,
                "kind": feature_block_kind(placed_name),
            })

        if block_name in RANDOMIZABLE_CONTAINERS:
            loot_table = None
            if isinstance(nbt, dict) and "LootTable" in nbt:
                loot_table = strip_namespace(str(nbt["LootTable"]))
            containers.append({
                "pos": pos,
                "block": strip_namespace(block_name),
                "loot_table": loot_table,
                # This preserves StructureTemplate's Random.nextLong order.
                "placement_index": placement_index,
            })

    result: Dict[str, Any] = {
        "name": template_name(entry_name, prefix),
        "size": size,
    }
    if jigsaws:
        result["jigsaws"] = jigsaws
    if containers:
        result["containers"] = containers
    if grass_path_positions:
        result["grass_paths"] = [
            {
                "pos": pos,
                "above_empty": (
                    (pos[0], pos[1] + 1, pos[2])
                    not in occupied_positions
                ),
            }
            for pos in grass_path_positions
        ]
    if feature_blocks:
        result["feature_blocks"] = feature_blocks
    return result


def sha1_file(path: Path) -> str:
    digest = hashlib.sha1()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def find_source_file(root: Path, filename: str) -> Path:
    direct = root / filename
    if direct.is_file():
        return direct
    matches = list(root.rglob(filename))
    if len(matches) != 1:
        raise NbtError(
            f"Expected exactly one {filename} below {root}, "
            f"found {len(matches)}"
        )
    return matches[0]


def balanced_call(text: str, token_start: int) -> Tuple[str, int]:
    """Return a Java call expression and its exclusive end offset.

    CFR writes each 1.16.1 pool registration on one very long line.  Some
    Village entries contain nested processor constructors, so a non-greedy
    regular expression cannot reliably identify the closing parenthesis.
    """

    opening = text.find("(", token_start)
    if opening < 0:
        raise NbtError("Java call has no opening parenthesis")
    depth = 0
    in_string = False
    escaped = False
    for index in range(opening, len(text)):
        char = text[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return text[token_start:index + 1], index + 1
            if depth < 0:
                break
    raise NbtError("Unbalanced Java call expression")


def split_call_arguments(call: str) -> List[str]:
    opening = call.find("(")
    if opening < 0 or not call.endswith(")"):
        raise NbtError("Malformed Java call")
    arguments: List[str] = []
    start = opening + 1
    depth = 1
    in_string = False
    escaped = False
    for index in range(start, len(call) - 1):
        char = call[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "," and depth == 1:
            arguments.append(call[start:index].strip())
            start = index + 1
    arguments.append(call[start:-1].strip())
    return arguments


def strip_object_casts(expression: str) -> str:
    expression = expression.strip()
    while expression.startswith("(Object)"):
        expression = expression[len("(Object)"):].lstrip()
    return expression


def iter_pair_calls(text: str):
    """Yield CFR Pair constructor calls in source order."""

    offset = 0
    tokens = ("new Pair(", "Pair.of(")
    while True:
        matches = [
            (text.find(token, offset), token)
            for token in tokens
        ]
        matches = [match for match in matches if match[0] >= 0]
        if not matches:
            return
        start, _ = min(matches)
        call, end = balanced_call(text, start)
        yield call
        offset = end


def parse_village_element(
    pair_call: str, filename: str, line_number: int
) -> Dict[str, Any]:
    arguments = split_call_arguments(pair_call)
    if len(arguments) != 2:
        raise NbtError(
            f"{filename}:{line_number}: Pair has {len(arguments)} arguments"
        )
    element = strip_object_casts(arguments[0])
    weight_text = strip_object_casts(arguments[1])
    weight_match = re.fullmatch(r"(\d+)", weight_text)
    if not weight_match:
        raise NbtError(
            f"{filename}:{line_number}: invalid pool weight {weight_text!r}"
        )
    weight = int(weight_match.group(1))
    if weight <= 0:
        raise NbtError(
            f"{filename}:{line_number}: non-positive pool weight"
        )

    legacy_token = "new LegacySinglePoolElement"
    if element.startswith(legacy_token):
        legacy_call, end = balanced_call(element, 0)
        if element[end:].strip():
            raise NbtError(
                f"{filename}:{line_number}: trailing Legacy element data"
            )
        legacy_arguments = split_call_arguments(legacy_call)
        if not legacy_arguments:
            raise NbtError(
                f"{filename}:{line_number}: empty Legacy element"
            )
        template_match = re.fullmatch(
            r'"([^"]+)"', legacy_arguments[0].strip()
        )
        if not template_match:
            raise NbtError(
                f"{filename}:{line_number}: invalid Legacy template"
            )
        result: Dict[str, Any] = {
            "type": "legacy",
            "template": template_match.group(1),
            "weight": weight,
        }
        if len(legacy_arguments) > 2:
            raise NbtError(
                f"{filename}:{line_number}: Legacy element has too many "
                "arguments"
            )
        if len(legacy_arguments) == 2:
            processors = legacy_arguments[1].strip()
            identifier = re.search(
                r"\b(immutableList\d*)\s*$", processors
            )
            if identifier:
                result["processors"] = (
                    filename + "#" + identifier.group(1)
                )
            else:
                result["processors"] = (
                    filename + "#inline:"
                    + hashlib.sha1(
                        processors.encode("utf-8")
                    ).hexdigest()
                )
        return result

    feature_token = "new FeaturePoolElement"
    if element.startswith(feature_token):
        feature_call, end = balanced_call(element, 0)
        if element[end:].strip():
            raise NbtError(
                f"{filename}:{line_number}: trailing Feature element data"
            )
        feature_arguments = split_call_arguments(feature_call)
        if len(feature_arguments) != 1:
            raise NbtError(
                f"{filename}:{line_number}: Feature element has "
                f"{len(feature_arguments)} arguments"
            )
        # The configured feature does not have a structure-template NBT.
        # Its expression is retained as an identity; all such elements have a
        # point-sized box and a single non-connecting jigsaw in 1.16.1.
        return {
            "type": "feature",
            "feature": re.sub(r"\s+", "", feature_arguments[0]),
            "weight": weight,
        }

    if element == "EmptyPoolElement.INSTANCE":
        return {
            "type": "empty",
            "weight": weight,
        }

    raise NbtError(
        f"{filename}:{line_number}: unknown Village pool element "
        f"{element[:80]!r}"
    )


def extract_village_pools(
    source_root: Path, template_names
) -> Dict[str, Any]:
    """Read the five hard-coded 1.16.1 Village pool registries."""

    pool_pattern = re.compile(
        r"new StructureTemplatePool\("
        r'new ResourceLocation\("([^"]+)"\),\s*'
        r'new ResourceLocation\("([^"]+)"\),'
    )
    projection_pattern = re.compile(
        r"StructureTemplatePool\.Projection\.([A-Z_]+)"
    )

    pools: List[Dict[str, Any]] = []
    source_sha1: Dict[str, str] = {}
    type_counts = {
        "legacy": 0,
        "feature": 0,
        "empty": 0,
    }
    raw_count = 0
    expanded_count = 0
    for filename in VILLAGE_POOL_FILES:
        source_file = find_source_file(source_root, filename)
        source_sha1[filename] = sha1_file(source_file)
        text = source_file.read_text(encoding="utf-8")
        for line_number, line in enumerate(text.splitlines(), 1):
            if "new StructureTemplatePool(" not in line:
                continue
            pool_match = pool_pattern.search(line)
            projections = projection_pattern.findall(line)
            if not pool_match or len(projections) != 1:
                raise NbtError(
                    f"{source_file}:{line_number}: could not parse pool header"
                )
            elements = [
                parse_village_element(pair, filename, line_number)
                for pair in iter_pair_calls(line)
            ]
            if not elements:
                raise NbtError(
                    f"{source_file}:{line_number}: pool has no elements"
                )
            for element in elements:
                type_counts[element["type"]] += 1
                raw_count += 1
                expanded_count += element["weight"]
                if (
                    element["type"] == "legacy"
                    and (
                        element["template"][len("village/"):]
                        if element["template"].startswith("village/")
                        else element["template"]
                    ) not in template_names
                ):
                    raise NbtError(
                        f"{source_file}:{line_number}: missing Village "
                        f"template {element['template']}"
                    )
            pools.append({
                "name": pool_match.group(1),
                "fallback": pool_match.group(2),
                "projection": projections[0].lower(),
                "elements": elements,
            })

    names = [pool["name"] for pool in pools]
    projection_counts = {
        projection: sum(
            pool["projection"] == projection for pool in pools
        )
        for projection in ("rigid", "terrain_matching")
    }
    expected_types = {
        "legacy": 591,
        "feature": 35,
        "empty": 22,
    }
    if (
        len(pools) != 61
        or len(names) != len(set(names))
        or raw_count != 648
        or expanded_count != 2949
        or type_counts != expected_types
        or projection_counts != {
            "rigid": 44,
            "terrain_matching": 17,
        }
    ):
        raise NbtError(
            "Unexpected Village pool registry: "
            f"{len(pools)} pools, {len(set(names))} unique names, "
            f"{raw_count} entries, {expanded_count} expanded, "
            f"types={type_counts}, projections={projection_counts}"
        )
    required_starts = {
        "village/plains/town_centers",
        "village/desert/town_centers",
        "village/savanna/town_centers",
        "village/snowy/town_centers",
        "village/taiga/town_centers",
    }
    if not required_starts.issubset(names):
        raise NbtError("Village start pool is missing")

    return {
        "source_sha1": source_sha1,
        "definitions": pools,
    }


def extract_bastion_pools(source_root: Path) -> Dict[str, Any]:
    """Read the hard-coded 1.16.1 Bastion pool bootstrap definitions.

    Minecraft 1.16.1 does not store these pools as data-pack JSON. The input
    directory must contain sources produced from the official jar and official
    Mojang mappings; the source/mappings themselves are not copied to output.
    """

    pool_pattern = re.compile(
        r"new StructureTemplatePool\("
        r'new ResourceLocation\("([^"]+)"\),\s*'
        r'new ResourceLocation\("([^"]+)"\),'
    )
    entry_pattern = re.compile(
        r"(?:Pair\.of|new Pair)\("
        r'\(Object\)new SinglePoolElement\("([^"]+)".*?\),\s*'
        r"\(Object\)(\d+)\)"
    )
    projection_pattern = re.compile(
        r"StructureTemplatePool\.Projection\.([A-Z_]+)"
    )

    pools: List[Dict[str, Any]] = []
    source_sha1: Dict[str, str] = {}
    pair_count = 0
    for filename in BASTION_POOL_FILES:
        source_file = find_source_file(source_root, filename)
        source_sha1[filename] = sha1_file(source_file)
        text = source_file.read_text(encoding="utf-8")
        for line_number, line in enumerate(text.splitlines(), 1):
            if "new StructureTemplatePool(" not in line:
                continue
            pool_match = pool_pattern.search(line)
            projections = projection_pattern.findall(line)
            if not pool_match or len(projections) != 1:
                raise NbtError(
                    f"{source_file}:{line_number}: could not parse pool header"
                )
            line_pair_count = line.count("Pair.of(") + line.count("new Pair(")
            entries = [
                {"template": template, "weight": int(weight)}
                for template, weight in entry_pattern.findall(line)
            ]
            if len(entries) != line_pair_count:
                raise NbtError(
                    f"{source_file}:{line_number}: parsed {len(entries)} of "
                    f"{line_pair_count} pool elements (unknown element type)"
                )
            pair_count += line_pair_count
            pools.append({
                "name": pool_match.group(1),
                "fallback": pool_match.group(2),
                "projection": projections[0].lower(),
                "elements": entries,
            })

    names = [pool["name"] for pool in pools]
    if len(pools) != 63 or len(names) != len(set(names)):
        raise NbtError(
            "Unexpected Bastion pool registry: "
            f"{len(pools)} pools, {len(set(names))} unique names"
        )
    if pair_count != 176:
        raise NbtError(
            f"Unexpected Bastion raw pool-element count: {pair_count}"
        )
    if any(pool["projection"] != "rigid" for pool in pools):
        raise NbtError("Bastion 1.16.1 contains a non-rigid pool")
    required_starts = {
        "bastion/units/base",
        "bastion/hoglin_stable/origin",
        "bastion/treasure/starters",
        "bastion/bridge/start",
    }
    if not required_starts.issubset(names):
        raise NbtError("Bastion start pool is missing")

    return {
        "source_sha1": source_sha1,
        "definitions": pools,
    }


def extract_jar(
    jar_path: Path,
    version: str,
    decompiled_source: Path = None,
) -> Dict[str, Any]:
    structures: Dict[str, List[Dict[str, Any]]] = {
        key: [] for key in STRUCTURE_PREFIXES
    }
    with zipfile.ZipFile(str(jar_path), "r") as archive:
        names = set(archive.namelist())
        for structure, prefix in STRUCTURE_PREFIXES.items():
            entries = sorted(
                name for name in names
                if name.startswith(prefix) and name.endswith(".nbt")
            )
            for entry_name in entries:
                root = read_structure_nbt(
                    archive.read(entry_name), entry_name
                )
                structures[structure].append(
                    extract_template(root, entry_name, prefix)
                )

    result = {
        "format": 1,
        "minecraft_version": version,
        "jar_sha1": sha1_file(jar_path),
        "structures": structures,
    }
    if decompiled_source is not None:
        village_names = {
            template["name"]
            for template in structures["village"]
        }
        result["pools"] = {
            "bastion": extract_bastion_pools(decompiled_source),
            "village": extract_village_pools(
                decompiled_source, village_names
            ),
        }
    return result


def totals(manifest: Dict[str, Any]) -> Dict[str, Dict[str, int]]:
    result: Dict[str, Dict[str, int]] = {}
    for structure, templates in manifest["structures"].items():
        result[structure] = {
            "templates": len(templates),
            "jigsaws": sum(
                len(template.get("jigsaws", []))
                for template in templates
            ),
            "containers": sum(
                len(template.get("containers", []))
                for template in templates
            ),
            "loot_containers": sum(
                1
                for template in templates
                for container in template.get("containers", [])
                if container["loot_table"] is not None
            ),
        }
    return result


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Extract village/bastion jigsaw and container facts from an "
            "official Minecraft Java jar"
        )
    )
    parser.add_argument("--version", default="1.16.1")
    parser.add_argument(
        "--jar",
        type=Path,
        help="official client/server jar (defaults to the launcher install)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build-structure-data") / "jigsaw-1.16.1.json",
    )
    parser.add_argument(
        "--decompiled-source",
        type=Path,
        help=(
            "directory containing Mojang-mapped 1.16.1 decompiled sources; "
            "needed to extract hard-coded Bastion pools"
        ),
    )
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    jar_path = args.jar or official_jar_path(args.version)
    if not jar_path.is_file():
        print(f"Official Minecraft jar was not found: {jar_path}", file=sys.stderr)
        return 2

    try:
        manifest = extract_jar(
            jar_path, args.version, args.decompiled_source
        )
    except (OSError, zipfile.BadZipFile, NbtError) as error:
        print(f"Extraction failed: {error}", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(
            manifest,
            stream,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        )
        stream.write("\n")

    print(f"Wrote {args.output}")
    for structure, summary in totals(manifest).items():
        print(
            f"  {structure}: {summary['templates']} templates, "
            f"{summary['jigsaws']} jigsaws, "
            f"{summary['containers']} containers "
            f"({summary['loot_containers']} with LootTable)"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
