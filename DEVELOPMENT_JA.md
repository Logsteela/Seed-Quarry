# Seed Atlas 改造用メモ

## このPCに導入済みのQt

現在は次の環境を自動検出して使用します。追加インストールは不要です。

- Qt `6.10.1`: `C:\Qt\6.10.1\mingw_64`
- MinGW `13.1.0`: `C:\Qt\Tools\mingw1310_64`

`dev-build.ps1`は、互換性確認済みのQt 6.8系を優先し、無い場合は
`C:\Qt\6.*\mingw_64`にあるQt 6を使用します。

## 別のPCへ初めて導入する場合

公式インストーラー:

<https://download.qt.io/official_releases/online_installers/qt-online-installer-windows-x64-online.exe>

インストーラーでは`Custom installation`を選び、標準の`C:\Qt`へ次を導入します。

- `Qt 6.8.x` → `MinGW 13.1.0 64-bit`
- `Developer and Designer Tools` → `MinGW 13.1.0 64-bit`

Qt Creatorは好みで追加できますが、下記スクリプトだけでビルド・起動できるため
必須ではありません。Android、WebAssembly、MSVC、Debug Information、
Qtのソース一式も、このプロジェクトには不要です。

## 普段の実行

PowerShellでこのフォルダーを開き、次を実行します。

```powershell
.\dev-build.ps1
```

最初にソースの静的検査とCubiomesの回帰テストを実行し、その後でSeed Atlasを
ビルドします。初回だけ全体をビルドし、2回目以降は変更されたファイルだけを
増分ビルドして、完了後に開発版Seed Atlasを直接起動します。同時コンパイルは
メモリ不足を避けるため最大2個です。配布用EXEを毎回作る必要はありません。

ビルド後は`windeployqt`も自動実行し、QtのDLLと`platforms`プラグインを
EXEの隣へ配置します。その後は次のファイルをエクスプローラーから
ダブルクリックして起動できます。

```text
build-dev-debug\debug\seed-atlas.exe
```

### デスクトップショートカット

ビルド済みEXEがある状態で、次を一度実行します。この処理はデスクトップへ
直接書き込まず、プロジェクト内に移動用ファイルを作ります。

```powershell
.\make-desktop-shortcuts.ps1
```

`desktop-shortcuts`フォルダーに次の2個が作成されます。必要なものを
エクスプローラーでデスクトップへ移動してください。

- `Seed Atlas - Run`: 現在のビルドをすぐに起動
- `Seed Atlas - Rebuild and Run`: 変更部分を再ビルドし、Qt DLLを再配置して起動

ショートカットを削除した場合やプロジェクトの保存場所を移動した場合も、
新しい保存場所で上記スクリプトをもう一度実行してから、作成されたリンクを
デスクトップへ移せば更新できます。再ビルドに失敗した場合はPowerShell画面を
閉じず、エラーを表示します。

起動せずビルドだけ行う場合:

```powershell
.\dev-build.ps1 -NoRun
```

qmakeからやり直す場合:

```powershell
.\dev-build.ps1 -Reconfigure
```

一時的にCubiomesの回帰テストを省略する場合:

```powershell
.\dev-build.ps1 -SkipTests
```

Qtランタイムの再配置も省略して、コンパイルだけを最短で行う場合:

```powershell
.\dev-build.ps1 -NoRun -SkipDeploy
```

Qtをまだ導入していない状態でも、次の静的検査だけは実行できます。

```powershell
.\check-source.ps1
```

この検査はUI XML、UTF-8、Qtオブジェクト名、C++からのUI参照、追加条件値の
配線、ビルドスクリプトの構文を確認します。

## 現在追加済みのGUI検索条件

- 荒廃したポータル: 地下、空洞、巨大、左右反転、生成カテゴリ、回転、
  既存の開始テンプレート
- 村: 回転、既存の廃村・開始テンプレート
- 砦の遺跡: 回転、既存の開始テンプレート
- イグルー: 入口の向き、地下へ続く中間部の長さ、既存の地下室
- 古代都市: 中央テンプレート、回転
- 試練の間: 開始テンプレート、回転
- 砂漠の寺院、ジャングルの寺院、沼地の小屋: 入口の向き
- 砂漠の寺院、難破船、埋もれた宝、荒廃したポータル
  （Java 1.16.1 / 1.16.5）: チェスト内容による検索
- Other → `範囲内の構造物Loot合計`（Java 1.16.1 / 1.16.5）:
  Location内の対象構造物すべてを合計したチェスト内容による検索

Loot条件では、アイテム条件を任意の個数だけ追加できます。GUI上で次を選べます。

- 条件同士をすべて満たす（AND）/ いずれかを満たす（OR）
- 複数の構造物のいずれか / 各構造物それぞれ / 全構造物の合計
- 構造物内の全チェスト合計 / いずれか1個 / 各チェストそれぞれ /
  Vanilla内部の生成順チェスト1～4
- アイテム個数の最小値・最大値（最大値は上限なしも可）
- エンチャント本の種類とレベル範囲

上記4種類に加え、村（画面の1.16、内部は1.16.1計算）と砦の遺跡
（内部バージョン1.16.1限定）の計算を追加しています。村には未解決の
配置RNGがあり、その場合はLoot一致と確定せず保留します。ジャングルの寺院や
沼地の小屋で同じ詳細ページを開くと、未対応である旨を表示してLoot欄を無効化
します。未検証の結果は流用しません。

可変個のLoot条件は、従来の固定長Conditionを変更せず、条件文字列の末尾に
バージョン付き拡張データとして保存します。このため、従来のセッションと
プリセットはそのまま読み込め、新しい条件もセッション、プリセット、
コピー＆貼り付けに含まれます。

### 48-bit検索のLoot事前判定と高速化

検索方式を`48-bit only`または`48-bit family blocks`にすると、検索ボタン横の
`Loot高速化（48-bit検索）`を選択できます。既定はOFFです。

ONの場合は次の順で処理します。

1. 下位48bitから構造物の座標候補を確認
2. 候補座標のLootを事前判定
3. family blocksでは、各上位16bitについてバイオーム、実際の生成可否、
   荒廃したポータル等のバリアント、実際の難破船型を確定

`48-bit only`は、上位16bitのいずれかで成立し得る候補を返します。難破船は
海中型と浜辺型の両方を試し、どちらもLoot条件を満たさない場合だけ除外します。
全構造物Loot合計は、候補のうち実際に生成される集合がまだ不明なため、
偽陰性を出さない合計値の下限・上限で、安全に不可能と分かる候補だけを
除外します。

`48-bit family blocks`では、48bit段階のLoot結果を後続の65536 Seedで
再利用します。最終的な検索結果は各64-bit Seedの実際のバイオームと
viabilityで確定します。以下の固定構造物のキャッシュは厳密な再利用です。

村Lootの高速化は別方式です。1スレッドに1つの48-bit familyを割り当て、
村Lootを除いた条件に合う64-bit SeedでLootを確認します。確定不一致なら
同じfamilyの残りを省略します。これは非網羅検索で、該当Seedを見落とす
可能性があります。全探索したい場合はLoot高速化をOFFにしてください。
一致した場合は結果として出力し、停止設定に応じて検索を継続します。
未確定のLootはfamily打ち切りの根拠にしません。

NOT・ORゲート・Lua、または村Lootの除外条件（個数0以下）を含む検索では、
村のfamily省略を自動的に無効にします。アイテム欄のAND/ORは対象外で、
高速化と併用できます。48-bit onlyでは村Lootはまだ絞り込みに使いません。

- 砂漠の寺院、埋もれた宝、荒廃したポータル:
  同じ下位48bitと構造物座標につき、workerごとに1回
- 難破船:
  浜辺型と海中型で計算が変わるため、それぞれ1回（最大2回）

下位48bitが次の値へ変わるとキャッシュを破棄するため、メモリは一家族分に
限定されます。設定はSessionの`#FastLoot48: 0/1`へ保存され、
ヘッドレス検索でも同じ動作になります。

Condition Add/Edit画面の詳細部分はスクロール可能です。General、Location、
OK/Cancelは固定表示され、初期ウィンドウサイズも画面の使用可能範囲内へ
収めます。

荒廃したポータル、村、砦の遺跡、イグルーについては、Java 1.16.1を
明示的に通す回帰テストも追加しています。特に砦の遺跡は1.16.1だけにある
開始テンプレートと回転の入れ替わりを1.16.5との比較で検査します。

## Git

このソースはGit管理済みです。

- `master`: 受領したSeed Atlas 4.2.dev0そのまま
- `codex/portal-variants`: 今回の改造

よく使う確認コマンド:

```powershell
git status
git diff
git log --oneline --decorate -10
```

変更を保存するコミット:

```powershell
git add .gitignore DEVELOPMENT_JA.md LOOT_INTEGRATION_JA.md `
    cubiomes/loot.c cubiomes/loot.h cubiomes/tests_versions.c `
    seed-atlas.pro src/conditiondialog.cpp src/conditiondialog.h `
    src/lootcondition.cpp src/lootcondition.h `
    src/lootconditionwidget.cpp src/lootconditionwidget.h `
    src/scripts.cpp src/search.cpp src/search.h `
    test-loot.ps1 tests
git commit -m "変更内容を日本語で書く"
```

直前のコミット以降に変更されたファイルを元へ戻す操作は内容を失うため、
実行前に`git diff`で対象を確認してください。

## Luaによる再ビルド不要の条件

正式なGUI項目の追加にはC++の再ビルドが必要です。一方、Luaフィルターは
`.lua`ファイルを編集するだけで変更できます。

今回追加する`getStructureVariant(type, x, z [, biome])`を使うと、構造物の
内部バリアントをLuaから取得できます。戻り値には次のフィールドがあります。

`abandoned`, `giant`, `underground`, `airpocket`, `basement`, `cracked`,
`size`, `start`, `biome`, `rotation`, `mirror`, `x`, `y`, `z`, `sx`, `sy`, `sz`

荒廃したポータルが地下型か調べる例:

```lua
function check(seed, at, branches)
    local v = getStructureVariant(Ruined_Portal, at.x, at.z)
    if v and v.underground then
        return at.x, at.z
    end
end
```

### 構造物のチェスト条件（Java 1.16.1 / 1.16.5）

GUIの各構造物条件では、次の種類について`チェスト内容の条件`を有効に
できます。

- 砂漠のピラミッド
- 難破船
- 埋もれた宝
- 荒廃したポータル（オーバーワールド / ネザー）

村では画面の1.16を選択できます。砦の計算は内部の1.16.1限定のため、
通常の1.16選択欄からの利用はまだ未対応です。村・砦のチェスト座標条件は
絶対座標、または「Location基準点との差（X/Z）」を指定できます。
Location基準でもYはワールド絶対座標です。

難破船は、浜辺型か海中型か、テンプレート、回転、実際のチェストが属する
チャンクまで計算します。チェストは`物資`、`地図`、`宝物`から選択できます。
構造によって存在しない種類のチェストは、その構造物では不成立になります。
埋もれた宝と荒廃したポータルは1チェストです。

Otherの`範囲内の構造物Loot合計`でも対応構造物を選択でき、Location内の全構造物を
合計した個数、いずれかの構造物、各構造物それぞれ、AND / ORを指定できます。

### Lua: 砂漠の寺院のチェスト

`getDesertPyramidLoot(x, z [, chest])`で、指定した砂漠の寺院のチェスト内容を
取得できます。`x, z`は構造物のブロック座標です。

- `chest`に1～4を指定: Vanilla内部のRNG順に対応する1個のチェストを返す
- `chest`を省略: `[1]`～`[4]`の4個と、その合計である`total`を返す
- 未対応バージョン、範囲外、無効なチェスト番号: `nil`を返す

各チェストはアイテム名をキー、個数を値とするテーブルです。現在のキーは
`diamond`, `iron_ingot`, `gold_ingot`, `emerald`, `bone`, `spider_eye`,
`rotten_flesh`, `saddle`, `iron_horse_armor`, `golden_horse_armor`,
`diamond_horse_armor`, `enchanted_book`, `golden_apple`,
`enchanted_golden_apple`, `gunpowder`, `string`, `sand`です。

さらに`enchanted_books`には、エンチャント名ごとのテーブルが入ります。
レベルは`[1]`～`[5]`、その種類の合計は`total`です。例えば
`loot.total.enchanted_books.silk_touch[1]`でシルクタッチIの本の冊数を
取得できます。

4個のどこかにダイヤが1個以上ある例:

```lua
function check(seed, at, branches)
    local loot = getDesertPyramidLoot(at.x, at.z)
    if loot and loot.total.diamond >= 1 then
        return at.x, at.z
    end
end
```

同じチェストにダイヤと金のリンゴが入る例:

```lua
function check(seed, at, branches)
    local loot = getDesertPyramidLoot(at.x, at.z)
    if not loot then return nil end
    for chest = 1, 4 do
        if loot[chest].diamond >= 1 and loot[chest].golden_apple >= 1 then
            return at.x, at.z
        end
    end
end
```

詳細な出典、既知の制限、参照テスト値は`LOOT_INTEGRATION_JA.md`にあります。

## Loot条件のテスト

通常の全体ビルドとC回帰テスト:

```powershell
.\dev-build.ps1 -NoRun
```

可変長ルール、AND/OR、チェスト集計、範囲合計、エンチャントの単体テストと、
実際の検索スレッドを通す結合テスト:

```powershell
.\test-loot.ps1
```

すでにアプリをビルド済みで、Lootテスト側だけを再実行するなら
`.\test-loot.ps1 -SkipAppBuild`を使えます。

`tests\loot_integration_session.txt`と各`loot_*_integration_session.txt`は、
固定Seedを実際の検索スレッドへ通す結合テスト用セッションです。
`loot_integration_fail_session.txt`はダイヤ999個という不成立条件で、
同じSeedが除外されることを確認します。
