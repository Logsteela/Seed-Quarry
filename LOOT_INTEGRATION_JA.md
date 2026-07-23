# 1.16ルート計算の統合記録

## 現在の対応範囲

最初の移植対象として、Java 1.16.1 / 1.16.5の砂漠の寺院にある4チェストを
実装しました。Seed Atlasの検索スレッド内でCの計算を直接呼ぶため、外部の
JavaプロセスやMineMapの起動は不要です。

結果はアイテムごとの合計個数です。チェスト内のスロット位置は扱いません。
エンチャント本は個数まで正確に乱数を進めますが、現在は個別の
エンチャント名・レベルを結果へ出していません。

## 参照した固定バージョン

作業フォルダー直下に、確認時点の参照ソースを固定してあります。

| 参照物 | バージョン / コミット |
|---|---|
| MineMap | 1.0.26の配布JAR、ソース`28a643bcc2881fc0d8cceb83ece07abeef6a920d` |
| SeedFinding mc_feature_java | 1.171.1、`700d15fc65653c83cdffb00a98f143e6bba7a260` |
| SeedFinding mc_core_java | 1.192.1、`173a507ca33f33d081c575352bec335ee507c3c1` |
| SeedFinding mc_seed_java | 1.171.1、`a7543ead9132421b19c4a4d5e17219a8c309d929` |
| SeedFinding mc_math_java | 1.171.0、`4c59c2ebdd41d12c6322d21d5f4b4427e6d2107f` |

MineMap自身のLootクラスは、ルートテーブルを再実装せずSeedFindingのJava
ライブラリを呼び出しています。そのため、C移植の基準はMineMapの画面表示だけ
ではなく、上記の正確な依存バージョンの処理としました。

## 計算手順

砂漠の寺院のチャンク座標を`chunkX, chunkZ`とすると、1.16では次の順です。

1. ワールドSeedと`chunkX * 16, chunkZ * 16`からpopulation seedを生成
2. decoration salt `40003`を加えてJava Randomを初期化
3. 対象チェストの内部RNG index（0～3）に応じて乱数を進める
4. `nextLong()`を各チェストのloot seedとして使用
5. 1個目のloot poolを2～4回、2個目を4回抽選して個数を合計

`getDesertPyramidLoot(x, z [, chest])`の`chest`はLua向けに1～4です。
これは物理的な北・南・東・西ではなく、Vanilla内部のRNG順です。参照Javaの
砂漠の寺院generatorにも実座標対応のFIXMEが残っているため、確認できるまでは
方角名を付けません。

## 一致確認用の既知値

MineMap 1.0.26と固定したSeedFinding依存ライブラリから、次の値を生成しました。

- world seed: `3515201313347228787`
- 砂漠の寺院のchunk: `(17, -9)`
- Minecraft: Java 1.16.1

| RNG index | 主な結果（0個は省略） |
|---:|---|
| 0 | spider_eye 3, rotten_flesh 9, gunpowder 8, sand 1 |
| 1 | emerald 1, bone 5, saddle 1, iron_horse_armor 1, enchanted_golden_apple 1, sand 9 |
| 2 | gold_ingot 3, bone 15, spider_eye 1, string 1, sand 4 |
| 3 | diamond 1, emerald 2, bone 20, rotten_flesh 6, enchanted_book 1, gunpowder 4 |

この値を`cubiomes/tests_versions.c`の回帰テストへ固定しました。Cコードとは別に
Java Randomを再現した検算でも4チェストすべてが一致しています。

## Luaで選択できる条件

GUI項目を追加し直さなくても、現在のAPIだけで次を区別できます。

- 特定チェスト1～4、どれかのチェスト、4個の合計、4個すべて
- 1種類の最小個数・最大個数・完全一致
- アイテムを含む / 含まない
- 同じチェスト内に複数アイテムが揃う
- 複数チェスト全体で複数アイテムが揃う

将来GUI化する場合も、この全組み合わせを失わない設計にします。候補は
「対象アイテム」「比較方法」「個数」「対象チェスト（1～4・いずれか・合計・
すべて）」「同一チェストを要求」の組に分ける方式です。

## 次の移植候補と制限

MineMap 1.0.26で1.16.5向けに確認できる候補は、埋もれた宝、荒廃した
ポータル、難破船です。ソースにはエンドシティ用ルートもあります。
各構造物は、チェストごとのloot seedを得るまでの生成順やテンプレート配置が
違うため、砂漠の寺院のテーブルだけを差し替えて流用はしません。

1.20以降はルートテーブルや乱数源の変更をバージョンごとに再確認してから
別実装として追加します。現APIは未対応バージョンで`nil`を返すため、誤った
1.16結果を表示しません。
