# Minecraft Java 1.16.1 Village pool parser 調査メモ

## 結論

1.16.1 の5種類の村プールは、Bastion と同じ
`StructureTemplatePool` ではあるものの、要素は
`LegacySinglePoolElement` だけではない。

- 61 pools（全て一意）
- 648 raw entries
  - `LegacySinglePoolElement`: 591
  - `FeaturePoolElement`: 35
  - `EmptyPoolElement.INSTANCE`: 22
- weight 展開後: 2949 elements
- projection:
  - `RIGID`: 44 pools
  - `TERRAIN_MATCHING`: 17 pools
- fallback:
  - `empty`: 41 pools
  - 各 terminator pool: 20 pools

したがって、現在の Bastion 用 `entry_pattern` を
`LegacySinglePoolElement` に置き換えるだけでは不十分である。
最低でも Legacy / Feature / Empty の3型を区別し、processor、
projection、fallback、raw entry 順、weight を保存する必要がある。

## 調査対象

decompiled source root:

`C:\Users\home\AppData\Local\Temp\codex-seedfinder-DecompilerMC\src\1.16.1\client\net\minecraft`

| source | pool lines | SHA-1 |
|---|---:|---|
| `world/level/levelgen/feature/DesertVillagePools.java` | 38-49 | `cd1987546e276f333caf1a5ca93deef52847d9f0` |
| `world/level/levelgen/feature/PlainVillagePools.java` | 42-60 | `485df203785174eb2b6767e8dd5a4394cb58cf13` |
| `world/level/levelgen/feature/SavannaVillagePools.java` | 41-54 | `eb749c45c2a97ac76bd165bc9d0080c9a08b43fd` |
| `world/level/levelgen/feature/SnowyVillagePools.java` | 41-53 | `eff78072fcbf67d8c1dc806ba370b4e8912c1489` |
| `world/level/levelgen/feature/TaigaVillagePools.java` | 43-54 | `b4918e50d9ac65ac847f87ad5b5355cefbb61052` |

既存抽出器の Bastion 実装は
`tools/extract_jigsaw_data.py:310-391`。
特に単一正規表現の `entry_pattern` は同ファイルの323-327行、
raw `Pair` 数との照合は348-358行にある。

## 実際に現れる構文

### Pool

全61件は次の形で登録される。

```java
JigsawPlacement.POOLS.register(
    new StructureTemplatePool(
        new ResourceLocation("pool/id"),
        new ResourceLocation("fallback/id"),
        (List<Pair<StructurePoolElement, Integer>>)ImmutableList.of(...),
        StructureTemplatePool.Projection.RIGID
    )
);
```

現在の CFR 出力では1 pool が1物理行に収まっているが、
改行に依存せず `new StructureTemplatePool(` から対応する閉じ括弧まで
balanced scan する方が安全である。

### Pair wrapper

次の両方が混在する。

```java
new Pair((Object)ELEMENT, (Object)WEIGHT)
Pair.of((Object)ELEMENT, (Object)WEIGHT)
```

CFR が Java varargs を次のように途中で配列へ分割することもある。

```java
ImmutableList.of(
    (Object)new Pair(...),
    (Object[])new Pair[]{new Pair(...), Pair.of(...)}
)
```

`(Object[])new Pair[]{...}` 自体を要素に数えず、内側の
`new Pair` / `Pair.of` をraw順に抽出する必要がある。

### Legacy template

processor 無しと有りの2形がある。

```java
new LegacySinglePoolElement("village/...")
new LegacySinglePoolElement(
    "village/...",
    (List<StructureProcessor>)immutableList2
)
```

Plains には変数ではなく、ネストした `ImmutableList.of(new
RuleProcessor(...))` を第2引数へ直接渡す要素が4件ある。このため
`.*?` に依存する正規表現では括弧境界を安全に取得できない。

この5ファイルには `SinglePoolElement` と `ListPoolElement` のpool
entry は存在しない。

### Feature

```java
new FeaturePoolElement(
    Feature.TREE.configured(BiomeDefaultFeatures.NORMAL_TREE_CONFIG)
)
```

Feature はNBT template名を持たない。`FeaturePoolElement.java:60-84`
によれば、サイズ0、位置原点だけの合成jigsaw
（name=`minecraft:bottom`, target/pool=`minecraft:empty`）として
接続判定へ参加する。無視するとdecor connector周辺の候補列と
RNG消費がずれる。

現れる configured feature は次の13種。

| expression | raw occurrences |
|---|---:|
| `RANDOM_PATCH / CACTUS_CONFIG` | 2 |
| `BLOCK_PILE / HAY_PILE_CONFIG` | 6 |
| `TREE / NORMAL_TREE_CONFIG` | 3 |
| `FLOWER / PLAIN_FLOWER_CONFIG` | 2 |
| `TREE / ACACIA_TREE_CONFIG` | 3 |
| `BLOCK_PILE / MELON_PILE_CONFIG` | 2 |
| `TREE / SPRUCE_TREE_CONFIG` | 5 |
| `BLOCK_PILE / SNOW_PILE_CONFIG` | 2 |
| `BLOCK_PILE / ICE_PILE_CONFIG` | 2 |
| `TREE / PINE_TREE_CONFIG` | 2 |
| `BLOCK_PILE / PUMPKIN_PILE_CONFIG` | 2 |
| `RANDOM_PATCH / TAIGA_GRASS_CONFIG` | 2 |
| `RANDOM_PATCH / SWEET_BERRY_BUSH_CONFIG` | 2 |

### Empty

```java
Pair.of((Object)EmptyPoolElement.INSTANCE, (Object)5)
```

`EmptyPoolElement` は「読み飛ばす候補」ではない。
`JigsawPlacement.java:155-162` はprimary poolをweight展開してshuffleし、
fallback poolのshuffle結果を後置した後、最初のEmptyに到達した時点で
候補走査そのものを終了する。

## Weight と shuffle の注意

`StructureTemplatePool.java:58-68` は各raw entryをweight回だけ
raw順に配列へ複製する。`getShuffledTemplates` はその展開済み配列を
shuffleする（同86-87行）。

したがって、次を保持しなければ同じseedのpiece列にならない。

1. raw entry のソース順
2. element type
3. templateまたはfeature ID
4. processor指定
5. 個々のweight
6. pool projection
7. fallback

村全体でweightは正の整数 `1..150`。種類別の値は次の通り。

- Legacy: `1,2,3,4,5,6,7,10,11,49,50,98,100,150`
- Empty: `2,3,4,5,6,7,9,10`
- Feature: `1,2,4`

## 全pool照合表

`L/F/E` は Legacy / Feature / Empty のraw entry数。
`expanded` はweight合計。`weights` はソース順であり、これ自体を
抽出結果の検査値として利用できる。

### DesertVillagePools.java

| line | pool | fallback | proj | L/F/E | expanded | weights |
|---:|---|---|:---:|:---:|---:|---|
| 38 | `village/desert/town_centers` | `empty` | R | 6/0/0 | 250 | 98,98,49,2,2,1 |
| 39 | `village/desert/streets` | `village/desert/terminators` | T | 11/0/0 | 35 | 3,3,4,4,3,3,3,3,3,3,3 |
| 40 | `village/desert/zombie/streets` | `village/desert/zombie/terminators` | T | 11/0/0 | 35 | 3,3,4,4,3,3,3,3,3,3,3 |
| 42 | `village/desert/houses` | `village/desert/terminators` | R | 28/0/1 | 72 | 2,2,2,2,2,1,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,11,4,4,2,2,5 |
| 43 | `village/desert/zombie/houses` | `village/desert/zombie/terminators` | R | 28/0/1 | 68 | 2,2,2,2,2,1,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,7,4,4,2,2,5 |
| 44 | `village/desert/terminators` | `empty` | T | 2/0/0 | 2 | 1,1 |
| 45 | `village/desert/zombie/terminators` | `empty` | T | 2/0/0 | 2 | 1,1 |
| 46 | `village/desert/decor` | `empty` | R | 1/2/1 | 28 | 10,4,4,10 |
| 47 | `village/desert/zombie/decor` | `empty` | R | 1/2/1 | 28 | 10,4,4,10 |
| 48 | `village/desert/villagers` | `empty` | R | 3/0/0 | 12 | 1,1,10 |
| 49 | `village/desert/zombie/villagers` | `empty` | R | 2/0/0 | 11 | 1,10 |

### PlainVillagePools.java

| line | pool | fallback | proj | L/F/E | expanded | weights |
|---:|---|---|:---:|:---:|---:|---|
| 42 | `village/plains/town_centers` | `empty` | R | 8/0/0 | 204 | 50,50,50,50,1,1,1,1 |
| 44 | `village/plains/streets` | `village/plains/terminators` | T | 16/0/0 | 49 | 2,2,2,4,4,7,7,3,4,2,1,2,2,2,2,3 |
| 45 | `village/plains/zombie/streets` | `village/plains/terminators` | T | 16/0/0 | 49 | 2,2,2,4,4,7,7,3,4,2,1,2,2,2,2,3 |
| 47 | `village/plains/houses` | `village/plains/terminators` | R | 36/0/1 | 87 | 2,2,2,2,2,1,2,3,2,2,2,2,2,2,2,2,2,2,2,1,5,1,2,2,2,2,2,2,4,4,1,1,5,1,3,1,10 |
| 48 | `village/plains/zombie/houses` | `village/plains/terminators` | R | 35/0/1 | 83 | 2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,2,2,2,2,1,3,1,2,2,2,2,2,2,4,4,1,1,5,3,1,10 |
| 49 | `village/plains/terminators` | `empty` | T | 4/0/0 | 4 | 1,1,1,1 |
| 50 | `village/plains/trees` | `empty` | R | 0/1/0 | 1 | 1 |
| 51 | `village/plains/decor` | `empty` | R | 1/3/1 | 7 | 2,1,1,1,2 |
| 52 | `village/plains/zombie/decor` | `empty` | R | 1/3/1 | 6 | 1,1,1,1,2 |
| 53 | `village/plains/villagers` | `empty` | R | 3/0/0 | 12 | 1,1,10 |
| 54 | `village/plains/zombie/villagers` | `empty` | R | 2/0/0 | 11 | 1,10 |
| 55 | `village/common/animals` | `empty` | R | 9/0/1 | 26 | 7,7,1,1,1,1,1,1,1,5 |
| 56 | `village/common/sheep` | `empty` | R | 2/0/0 | 2 | 1,1 |
| 57 | `village/common/cats` | `empty` | R | 10/0/1 | 13 | 1,1,1,1,1,1,1,1,1,1,3 |
| 58 | `village/common/butcher_animals` | `empty` | R | 4/0/0 | 8 | 3,3,1,1 |
| 59 | `village/common/iron_golem` | `empty` | R | 1/0/0 | 1 | 1 |
| 60 | `village/common/well_bottoms` | `empty` | R | 1/0/0 | 1 | 1 |

### SavannaVillagePools.java

| line | pool | fallback | proj | L/F/E | expanded | weights |
|---:|---|---|:---:|:---:|---:|---|
| 41 | `village/savanna/town_centers` | `empty` | R | 8/0/0 | 459 | 100,50,150,150,2,1,3,3 |
| 43 | `village/savanna/streets` | `village/savanna/terminators` | T | 19/0/0 | 56 | 2,2,4,7,3,4,4,4,4,4,1,2,2,2,2,2,2,2,3 |
| 44 | `village/savanna/zombie/streets` | `village/savanna/zombie/terminators` | T | 19/0/0 | 56 | 2,2,4,7,3,4,4,4,4,4,1,2,2,2,2,2,2,2,3 |
| 46 | `village/savanna/houses` | `village/savanna/terminators` | R | 31/0/1 | 81 | 2,2,2,2,2,2,2,2,2,2,2,2,2,2,7,1,3,2,2,2,2,2,2,2,3,4,6,4,2,2,2,5 |
| 47 | `village/savanna/zombie/houses` | `village/savanna/zombie/terminators` | R | 31/0/1 | 72 | 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,1,3,4,4,4,2,2,2,5 |
| 48 | `village/savanna/terminators` | `empty` | T | 5/0/0 | 5 | 1,1,1,1,1 |
| 49 | `village/savanna/zombie/terminators` | `empty` | T | 5/0/0 | 5 | 1,1,1,1,1 |
| 50 | `village/savanna/trees` | `empty` | R | 0/1/0 | 1 | 1 |
| 51 | `village/savanna/decor` | `empty` | R | 1/3/1 | 17 | 4,4,4,1,4 |
| 52 | `village/savanna/zombie/decor` | `empty` | R | 1/3/1 | 17 | 4,4,4,1,4 |
| 53 | `village/savanna/villagers` | `empty` | R | 3/0/0 | 12 | 1,1,10 |
| 54 | `village/savanna/zombie/villagers` | `empty` | R | 2/0/0 | 11 | 1,10 |

### SnowyVillagePools.java

| line | pool | fallback | proj | L/F/E | expanded | weights |
|---:|---|---|:---:|:---:|---:|---|
| 41 | `village/snowy/town_centers` | `empty` | R | 6/0/0 | 306 | 100,50,150,2,1,3 |
| 43 | `village/snowy/streets` | `village/snowy/terminators` | T | 16/0/0 | 47 | 2,2,2,2,4,4,4,7,4,4,1,2,2,2,2,3 |
| 44 | `village/snowy/zombie/streets` | `village/snowy/terminators` | T | 16/0/0 | 47 | 2,2,2,2,4,4,4,7,4,4,1,2,2,2,2,3 |
| 46 | `village/snowy/houses` | `village/snowy/terminators` | R | 30/0/1 | 68 | 2,2,2,3,2,2,2,2,2,2,2,2,2,2,2,3,1,1,2,2,2,2,2,2,2,2,3,3,2,2,6 |
| 47 | `village/snowy/zombie/houses` | `village/snowy/terminators` | R | 30/0/1 | 65 | 2,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,3,3,2,2,6 |
| 48 | `village/snowy/terminators` | `empty` | T | 4/0/0 | 4 | 1,1,1,1 |
| 49 | `village/snowy/trees` | `empty` | R | 0/1/0 | 1 | 1 |
| 50 | `village/snowy/decor` | `empty` | R | 3/3/1 | 27 | 4,4,1,4,4,1,9 |
| 51 | `village/snowy/zombie/decor` | `empty` | R | 3/3/1 | 22 | 1,1,1,4,4,4,7 |
| 52 | `village/snowy/villagers` | `empty` | R | 3/0/0 | 12 | 1,1,10 |
| 53 | `village/snowy/zombie/villagers` | `empty` | R | 2/0/0 | 11 | 1,10 |

### TaigaVillagePools.java

| line | pool | fallback | proj | L/F/E | expanded | weights |
|---:|---|---|:---:|:---:|---:|---|
| 43 | `village/taiga/town_centers` | `empty` | R | 4/0/0 | 100 | 49,49,1,1 |
| 45 | `village/taiga/streets` | `village/taiga/terminators` | T | 16/0/0 | 49 | 2,2,2,4,4,4,7,7,4,1,1,2,2,2,2,3 |
| 46 | `village/taiga/zombie/streets` | `village/taiga/terminators` | T | 16/0/0 | 49 | 2,2,2,4,4,4,7,7,4,1,1,2,2,2,2,3 |
| 48 | `village/taiga/houses` | `village/taiga/terminators` | R | 27/0/1 | 76 | 4,4,4,4,4,2,2,2,2,2,2,2,2,1,1,3,2,2,2,2,2,2,2,6,6,1,2,6 |
| 49 | `village/taiga/zombie/houses` | `village/taiga/terminators` | R | 26/0/1 | 74 | 4,4,4,4,4,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,6,6,1,2,6 |
| 50 | `village/taiga/terminators` | `empty` | T | 4/0/0 | 4 | 1,1,1,1 |
| 51 | `village/taiga/decor` | `empty` | R | 7/5/1 | 39 | 10,4,1,1,1,2,1,4,4,2,4,1,4 |
| 52 | `village/taiga/zombie/decor` | `empty` | R | 4/5/1 | 26 | 4,1,1,1,4,4,2,4,1,4 |
| 53 | `village/taiga/villagers` | `empty` | R | 3/0/0 | 12 | 1,1,10 |
| 54 | `village/taiga/zombie/villagers` | `empty` | R | 2/0/0 | 11 | 1,10 |

`R` は `RIGID`、`T` は `TERRAIN_MATCHING`。

## Processor 定義

processor変数名はファイルローカルである。同じ `immutableList` でも
バイオームごとに内容が違うため、manifest内のIDは
`PlainVillagePools#immutableList` のようにsource classでscopeするか、
正規化した式のhashを用いるべきである。

以下の使用数はweight展開前の Legacy entry 数。

### DesertVillagePools.java

- `immutableList`（37行、32 entries）:
  zombie化。doors / torch / wall torchをairへ変更し、
  sandstone系・terracottaを指定確率でcobwebへ変更。
  wheatをbeetroots（0.2）またはmelon stem（0.1）へ変更。
- `immutableList2`（41行、3 entries）:
  wheatをbeetroots（0.2）またはmelon stem（0.1）へ変更。
- processor無し: 60 entries。

### PlainVillagePools.java

- `immutableList`（40行、40 entries）:
  zombie化。cobblestoneのmossy化、doors・torch除去、
  cobweb化、glass pane変換、作物変換を行う。
- `immutableList2`（41行、26 entries）:
  cobblestoneを0.1でmossy cobblestoneへ変更。
- `immutableList3`（43行、36 entries）:
  road用。water上のgrass pathをoak planksへ変更し、
  path/grass/dirtを地形に応じてgrass blockまたはwaterへ変更。
- `immutableList4`（46行、2 entries）:
  wheatをcarrots（0.3）、potatoes（0.2）、
  beetroots（0.1）へ変更。
- inline mossify 0.2（42行、2 entries）:
  town centerのcobblestoneを0.2でmossy化。
- inline mossify 0.7（42・47行、2 entries）:
  town center/meeting pointのcobblestoneを0.7でmossy化。
- processor無し: 41 entries。

### SavannaVillagePools.java

- `immutableList`（40行、36 entries）:
  zombie化。doors・torch除去、acacia/terracotta/glassのcobweb化、
  glass pane変換、wheatからmelon stemへの変更。
- `immutableList2`（42行、48 entries）:
  road用。water上のgrass pathをacacia planksへ変更し、
  path/grass/dirtを地形に応じてgrass blockまたはwaterへ変更。
- `immutableList3`（45行、3 entries）:
  wheatを0.1でmelon stemへ変更。
- processor無し: 38 entries。

### SnowyVillagePools.java

- `immutableList`（40行、33 entries）:
  zombie化。doors・torch・wall torch・lantern除去、
  spruce/glassのcobweb化、glass pane変換、作物変換を行う。
- `immutableList2`（42行、36 entries）:
  road用。water上のgrass pathをspruce planksへ変更し、
  path/grass/dirtを地形に応じてgrass blockまたはwaterへ変更。
- `immutableList3`（45行、2 entries）:
  wheatをcarrots（0.1）またはpotatoes（0.8）へ変更。
- processor無し: 42 entries。

### TaigaVillagePools.java

- `immutableList`（41行、28 entries）:
  zombie化。cobblestoneのmossy化、doors・torch除去、
  campfire消火、cobweb化、glass pane変換、作物変換を行う。
  末尾の空 `ProcessorRule[]` はCFRのvarargs artifact。
- `immutableList2`（42行、27 entries）:
  cobblestoneを0.1でmossy cobblestoneへ変更。
- `immutableList3`（44行、36 entries）:
  road用。water上のgrass pathをspruce planksへ変更し、
  path/grass/dirtを地形に応じてgrass blockまたはwaterへ変更。
- `immutableList4`（47行、2 entries）:
  wheatをpumpkin stem（0.3）またはpotatoes（0.2）へ変更。
- processor無し: 16 entries。

## Chest位置・loot seed実装への影響

### RuleProcessor

`RuleProcessor.java:37-44` は各world block座標の
`Mth.getSeed(pos)` から独立した `Random` を作る。Structure placement
へ渡された共有Randomを消費しない。また、上記processorには
chest/barrel自体を対象にするruleがない。

したがって、村のpiece構成、chest X/Z、container順、LootTableSeedの
共有Random消費を再現する最初の段階では、RuleProcessorの内容を
manifestに識別可能な形で保持しつつ、block置換自体は省略できる。

### Projection

こちらは省略できない。`StructureTemplatePool.java:98-122` により
`TERRAIN_MATCHING` は
`GravityProcessor(WORLD_SURFACE_WG, -1)` を追加する。
`GravityProcessor.java:40-45` は各block（chestを含む）のYを、その
blockのworld X/Zにおけるsurface heightへ移す。

よって正確なchest Yには、piece開始地点の高さだけでなく、
各chest X/Zの `WORLD_SURFACE_WG` が必要である。

### LootTableSeed

`StructureTemplate.java:215-239` ではprocessor適用後のblock列を
元の順に配置し、randomizable containerに到達したとき共有Randomの
`nextLong()` をLootTableSeedへ設定する。今回のRuleProcessorは
containerを除去しないため、NBTから得た `placement_index` と
container順はそのまま重要である。

## 推奨するmanifest schema

最小限、各entryを次のいずれかとして保存すると扱いやすい。

```json
{
  "type": "legacy",
  "template": "village/plains/houses/plains_library_1",
  "processors": "PlainVillagePools#immutableList2",
  "weight": 5
}
```

```json
{
  "type": "feature",
  "feature": "tree",
  "configuration": "NORMAL_TREE_CONFIG",
  "weight": 1
}
```

```json
{
  "type": "empty",
  "weight": 10
}
```

pool側には `name`, `fallback`, `projection`, `elements` を保存する。

## Parser実装時の検査条件

次を全てassertすると、decompiler出力変化や未知のelement型を
黙って取りこぼしにくい。

1. source 5 files が各1件見つかる。
2. pool数61、pool名61 unique。
3. raw entries 648。
4. Legacy 591 / Feature 35 / Empty 22。
5. weight合計2949。
6. RIGID 44 / TERRAIN_MATCHING 17。
7. fallback `empty` 41 / non-empty 20。
8. 全weightが正の整数。
9. 全Legacy template IDが抽出済みVillage NBTに存在する。
10. 対応外element構文が1件でもあれば即時失敗する。
11. `FeaturePoolElement` は13種の既知configured featureだけ。
12. processor式は上記のscoped変数または4件の既知inline式だけ。

特に9は、例えばTaiga/Snowy terminator poolが自バイオームではなく
`village/plains/terminators/...` templateを参照する正常なケースを
誤って弾かないよう、全Village template集合に対して照合する。
