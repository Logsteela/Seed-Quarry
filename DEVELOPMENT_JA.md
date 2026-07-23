# Seed Atlas 改造用メモ

## 初回だけ必要なQt

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
増分ビルドして、完了後に開発版Seed Atlasを直接起動します。配布用EXEを毎回
作る必要はありません。

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

Qtをまだ導入していない状態でも、次の静的検査だけは実行できます。

```powershell
.\check-source.ps1
```

この検査はUI XML、UTF-8、Qtオブジェクト名、C++からのUI参照、追加条件値の
配線、ビルドスクリプトの構文を確認します。

配布用フォルダーが必要になった時だけ、公式`buildguide.md`にある
`windeployqt`を使用します。

## 現在追加済みのGUI検索条件

- 荒廃したポータル: 地下、空洞、巨大、左右反転、生成カテゴリ、回転、
  既存の開始テンプレート
- 村: 回転、既存の廃村・開始テンプレート
- 砦の遺跡: 回転、既存の開始テンプレート
- イグルー: 入口の向き、地下へ続く中間部の長さ、既存の地下室
- 古代都市: 中央テンプレート、回転
- 試練の間: 開始テンプレート、回転
- 砂漠の寺院、ジャングルの寺院、沼地の小屋: 入口の向き

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
git add src cubiomes/tests_versions.c dev-build.ps1 check-source.ps1 DEVELOPMENT_JA.md .gitignore
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

### 砂漠の寺院のチェスト（Java 1.16.1 / 1.16.5）

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
