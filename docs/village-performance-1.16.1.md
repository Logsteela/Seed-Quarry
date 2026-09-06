# Java 1.16.1 村検索の性能調査

## 結論

村の正確な Jigsaw 配置では WORLD_SURFACE_WG 高度計算が支配的である。
同じ 4×4 地形セルを囲む density column を村1件の間だけ再利用することで、
結果を変えずにレイアウト計算を約2.3倍高速化できた。

平坦高度や Cubiomes の連続近似高度を用いる案も測定したが、いずれも
「約90%の一致率で数倍高速」という基準を満たさなかった。そのため現時点では
近似モードを GUI に追加していない。検索結果は引き続き厳密計算だけを採用する。

## 測定結果

`tools/village_layout_probe.cpp` の deterministic benchmark を Release
ビルドで実行した。100件の seed、開始 chunk、5種類の村 biome を規則的に
変化させている。manifest 読み込みは計測前に warm up する。

### 厳密高度

変更前:

```text
count=100
total_ms=10547.224
per_layout_us=105472.242
```

density column キャッシュ後:

```text
count=100
total_ms=4614.007
per_layout_us=46140.065
```

同じ基準で約2.29倍。seedや村のpiece数により1件あたりの時間は変動する。

### 平坦高度

全座標で高さ64を返す場合:

```text
count=1000
total_ms=3568.536
per_layout_us=3568.536
```

厳密計算より十数倍速いが、100件の厳密レイアウトとの一致率は次だった。

```text
piece・回転・XZ・containerがすべて一致: 17%
containerの種類とXZがすべて一致:       25%
Loot table別のcontainer個数だけ一致:    26%
```

明らかに90%へ届かない。これを事前棄却へ使うと多数の正しいseedを見落とす。

### mapApproxHeight

49×49 quart-cell の連続近似高度を先に作る方式を20件で比較した。

```text
厳密レイアウトとの一致: 0%
厳密:  約42.8 ms/layout
近似:  約44.2 ms/layout
```

先読み領域が大きいため速度上の利点もなく、Jigsaw collisionのYが変わるため
piece列も一致しなかった。この方式も採用しない。

## 実装

`cubiomes/generator.c`:

- `getTerrainNoiseColumn116`
- `getFirstFreeHeightFromColumns116`

`src/villagestructure.cpp`:

- `CubiomesHeightContext` が `(noiseX, noiseZ)` ごとの33 density sampleを保持
- 1 block heightに必要な4本を再利用
- cacheは村レイアウト1件のローカル変数であり、seedを跨がない

したがって上位16bitや別seedへ誤って地形を流用しない。

`village_layout_probe --self-test` は、cache経路と従来の
`getFirstFreeHeight116` を毎回呼ぶstateless経路について、既知の2村の
全piece、bounding box、container座標が完全一致することも検査する。

## ベンチマーク

```powershell
.\build-village-probe\release\village_layout_probe.exe --benchmark 100
.\build-village-probe\release\village_layout_probe.exe --benchmark-flat 1000
.\build-village-probe\release\village_layout_probe.exe --compare-flat 100
.\build-village-probe\release\village_layout_probe.exe --compare-approx 20
.\build-village-probe\release\village_layout_probe.exe --self-test
```

Qt DLLがPATHにない端末では先に次を設定する。

```powershell
$env:Path = "C:\Qt\6.10.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:Path"
```

## 失敗からの注意

性能実験中、49×49 biome配列を通常の `QVector<int>` へ直接渡したところ
`genBiomes` が必要とする内部作業領域が不足し、probeがアクセス違反になった。
`genBiomes` の出力先は単純な `sx*sz` 配列とは限らず、必ず
`allocCache(generator, range)` で確保する必要がある。

この一括biome方式は性能改善が無かったため、最終実装から完全に除去した。
probeには `SEM_NOGPFAULTERRORBOX` 等も追加し、今後異常終了してもWindowsの
モーダルなクラッシュ画面を出さないようにした。

## 次の高速化候補

精度を維持するなら次の順で検討する。

同一world seed・開始座標・biomeの村を複数Loot条件が参照する場合については、
最終チェスト列のworker-localキャッシュを追加済み。上位16bitを跨がず、
最大256村・合計4096チェスト/workerなので結果の厳密性とメモリ上限を維持する。

1. 同一world seed内の異なる村でGeneratorとSurfaceNoiseを共有する。
2. search条件の安いbiome・viability・variant判定を必ず村layoutより前へ置く。
3. density column生成そのものをbatch化またはSIMD化する。
4. Loot条件が強い場合だけ使える、偽陰性のない下位48bit上限・下限判定を増やす。
5. 近似を再検討するなら「誤結果を返す」のではなく、近似で候補順位だけを付け、
   最後は必ず厳密計算する。ただし候補を棄却する用途には一致率の実証が必要。

平坦高度と現在のmapApproxHeightは棄却用途に使ってはいけない。
