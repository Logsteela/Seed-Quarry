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

初回だけ全体をビルドします。2回目以降は変更されたファイルだけを増分ビルドし、
完了後に開発版Seed Atlasを直接起動します。配布用EXEを毎回作る必要はありません。

起動せずビルドだけ行う場合:

```powershell
.\dev-build.ps1 -NoRun
```

qmakeからやり直す場合:

```powershell
.\dev-build.ps1 -Reconfigure
```

配布用フォルダーが必要になった時だけ、公式`buildguide.md`にある
`windeployqt`を使用します。

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
git add src dev-build.ps1 DEVELOPMENT_JA.md .gitignore
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
