# Minecraft Java 1.16.1 村 LootTableSeed 調査メモ

このメモは、公式 1.16.1 client jar を Mojang mappings で逆コンパイルした結果と、
SeedChecker 1.16.1 の Yarn 名 API を突き合わせたものである。村のチェストや樽に
付く `LootTableSeed` を C++ で再現する際に必要な共有乱数だけを整理する。

## チャンクごとの共有乱数の初期化

`ChunkGenerator.applyBiomeDecoration` は、生成対象チャンク `(chunkX, chunkZ)` ごとに
次の順で乱数を初期化する。

1. `blockX = chunkX * 16`, `blockZ = chunkZ * 16`
2. `decorationSeed = WorldgenRandom.setDecorationSeed(worldSeed, blockX, blockZ)`
3. 各 `GenerationStep.Decoration` で、各 structure/feature の直前に
   `setFeatureSeed(decorationSeed, index, step.ordinal())`

`setDecorationSeed` は Java の `Random` で world seed から
`nextLong() | 1` を 2 回取得し、次を計算して再度 `setSeed` する。

```text
decorationSeed = (blockX * oddA + blockZ * oddB) XOR worldSeed
```

各演算は Java の signed `long` と同じ 64-bit wraparound で行う。

1.16.1 の `StructureFeature` 登録順では、`SURFACE_STRUCTURES` 内の Village は
0 起点で `index=11`、同 step の ordinal は `4` である。したがって Village 配置直前の
外部 seed は次になる。

```text
villageFeatureSeed = decorationSeed + 11 + 10000 * 4
```

Java `Random` の内部 48-bit 状態へ変換するときは通常どおり、
次を用いる。

```text
(villageFeatureSeed XOR 0x5deece66d) & ((1L << 48) - 1)
```

これは村の開始チャンクではなく、対象チェストを含む「配置先チャンク」ごとに別である。

根拠となる主な公式メソッド:

- `ChunkGenerator.applyBiomeDecoration`
- `Biome.generate`
- `WorldgenRandom.setDecorationSeed`
- `WorldgenRandom.setFeatureSeed`
- `StructureFeature` の static 登録順
- `GenerationStep.Decoration`

## StructureStart と piece の順序

`Biome.generate` は Village 用の feature seed を設定した後、対象チャンクが参照する
Village start を順に `StructureStart.placeInChunk` へ渡す。

`StructureStart.placeInChunk` は start の `pieces` リストを格納順のまま走査し、
対象チャンクの bounding box と交差する piece だけへ同じ `WorldgenRandom` を渡す。
したがって基本的な再現順は次のとおり。

1. 対象チャンクが参照する Village start の stream 順
2. 各 start の piece index 順
3. template piece 内の NBT `blocks` 順

通常、対象チャンクを横切る Village start は 1 個だが、厳密な実装では複数 start も
考慮する必要がある。参照集合は `LongOpenHashSet` なので、複数 start 時の stream 順を
座標順と仮定してはいけない。

## template 内での LootTableSeed 付与

村の通常 template piece は `SinglePoolElement.place` から
`StructureTemplate.placeInWorld` を呼ぶ。処理順は次のとおり。

1. NBT の `blocks` リストを入力順のまま processor へ渡す
2. processor 後のリストを同じ順で走査する
3. 配置先チャンクの clip box 外にある block は飛ばす
4. block の配置に成功し、その block entity が
   `RandomizableContainerBlockEntity` なら共有乱数の `nextLong()` を 1 回呼ぶ
5. その値を NBT の `LootTableSeed` に設定して block entity を load する

manifest の `placement_index` は必ず保持する。同じ piece 内に複数 container がある
場合も、world 座標順ではなく `placement_index` 順に数える。別チャンクにある container
は、そのチャンク固有の乱数では処理されない。

`LootTable` のない空の樽も、block entity 型の判定時点では randomizable container
なので `nextLong()` を 1 回消費する。その後 `BarrelBlockEntity.load` は seed を保持
しないため、保存 NBT に `LootTableSeed` が現れない。この樽を乱数消費から除くと、
後続チェストの seed がずれる。

## Data marker は LootTableSeed の付与箇所ではない

`SinglePoolElement.place` は template 本体を配置した後、structure block の DATA marker
を列挙して `handleDataMarker` を呼ぶ。1.16.1 の Village element はこれを override
しておらず no-op である。村チェスト seed の付与箇所は DATA marker ではなく、
前節の `StructureTemplate.placeInWorld` にある block entity 処理である。

## 共有乱数を消費しない処理

- `StructurePlaceSettings.getRandomPalette`:
  piece 原点由来の独立した `new Random(Mth.getSeed(pos))`
- `RuleProcessor`:
  world block 座標由来の独立した `Random`
- `GravityProcessor`:
  高さを補正するが共有乱数は使わない
- 通常 block の配置、jigsaw replacement、Village の DATA marker:
  Village feature の共有乱数を直接消費しない

1.16.1 manifest 上で container を持つ 67 template は、すべて `RIGID` pool からだけ
参照される。したがって container の local 座標を piece 原点へ回転加算すればよく、
container 自身には `GravityProcessor` の Y 補正は入らない。

## FeaturePoolElement の注意

`FeaturePoolElement.place` は tree、block pile、random patch などの
`ConfiguredFeature.place` に同じ共有 `Random` をそのまま渡す。その 1-block bbox が
対象チャンク内にあり、piece 順でチェストより前なら、feature 固有の可変回数の乱数消費が
チェスト seed に影響する。

したがって完全な C++ 実装には、次のいずれかが必要である。

1. 該当する Village configured feature を正確に移植する
2. FeaturePoolElement が先行するチャンクだけ Java oracle へ委譲する
3. Oracle で十分な結果を集め、安全なケースを先に分類・検証する

単純に「先行 container 数だけ `nextLong()`」とする方法は、
FeaturePoolElement が先行するケースでは正確でない。

## 開発用 Oracle

`tools/VillageOracle1161.java --loot` は、
`build-structure-data/jigsaw-1.16.1.json` から生成済み piece に属する期待
chest/barrel 座標を計算する。その座標を含むチャンクだけを SeedChecker target level 8
まで生成し、次を出力する。

- `C`: 計画した piece/container/chunk 数
- `L`: 実在した container、座標、期待・実 LootTable、LootTableSeed
- `M`: manifest 上は期待したが生成後に存在しなかった container
- `U`: 生成された piece bbox 内にある manifest 外 container
- `E`: found/missing/unexpected 集計

実行例:

```powershell
.\tools\run-village-oracle-1.16.1.ps1 `
  -Seed 0 -ChunkX -25 -ChunkZ 21 -Loot
```

SeedChecker の公開 `getChunk(..., 8)` は内部で、対象 1 チャンクについても周辺 17×17 の
段階的なチャンク生成と参照構造物の生成を行う。既知ベクトル
`seed=0, start=(-25,21)` は `-Xmx1024m`、60 秒の軽い確認枠では完了しなかった。
そのため、この Oracle は大量検索に組み込まず、少数ベクトルの開発時照合専用とする。

runner は Java の working directory を
`build-structure-data/village-oracle` に固定している。SeedChecker が相対パスへ作る
中間ファイルはリポジトリ直下ではなく、この使い捨て領域にだけ残る。
