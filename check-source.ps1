param()

$ErrorActionPreference = "Stop"

$sourceDir = $PSScriptRoot
$uiPath = Join-Path $sourceDir "src\conditiondialog.ui"
$dialogPath = Join-Path $sourceDir "src\conditiondialog.cpp"
$searchPath = Join-Path $sourceDir "src\search.cpp"
$headerPath = Join-Path $sourceDir "src\search.h"
$scriptsPath = Join-Path $sourceDir "src\scripts.cpp"
$lootHeaderPath = Join-Path $sourceDir "cubiomes\loot.h"
$lootSourcePath = Join-Path $sourceDir "cubiomes\loot.c"
$lootConditionHeaderPath = Join-Path $sourceDir "src\lootcondition.h"
$lootConditionSourcePath = Join-Path $sourceDir "src\lootcondition.cpp"
$lootWidgetHeaderPath = Join-Path $sourceDir "src\lootconditionwidget.h"
$lootWidgetSourcePath = Join-Path $sourceDir "src\lootconditionwidget.cpp"
$villageStructureSourcePath = Join-Path $sourceDir "src\villagestructure.cpp"
$villageLootSeedSourcePath = Join-Path $sourceDir "src\villagelootseed.cpp"
$cubiomesMakefilePath = Join-Path $sourceDir "cubiomes\makefile"
$cubiomesCmakePath = Join-Path $sourceDir "cubiomes\CMakeLists.txt"
$projectPath = Join-Path $sourceDir "seed-quarry.pro"
$versionTestsPath = Join-Path $sourceDir "cubiomes\tests_versions.c"
$lootTestsPath = Join-Path $sourceDir "tests\lootcondition_tests.cpp"
$noticesPath = Join-Path $sourceDir "THIRD_PARTY_NOTICES.md"
$buildScriptPath = Join-Path $sourceDir "dev-build.ps1"
$lootTestScriptPath = Join-Path $sourceDir "test-loot.ps1"
$shortcutScriptPath = Join-Path $sourceDir "make-desktop-shortcuts.ps1"
$rebuildRunScriptPath = Join-Path $sourceDir "rebuild-and-run.ps1"
$formSearchUiPath = Join-Path $sourceDir "src\formsearchcontrol.ui"
$formSearchSourcePath = Join-Path $sourceDir "src\formsearchcontrol.cpp"
$configSourcePath = Join-Path $sourceDir "src\config.cpp"

function Assert-SourceCheck {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$uiText = $strictUtf8.GetString([System.IO.File]::ReadAllBytes($uiPath))
Assert-SourceCheck (-not $uiText.Contains([char]0xfffd)) `
    "conditiondialog.ui contains an invalid UTF-8 replacement character."

try {
    [xml]$uiXml = $uiText
}
catch {
    throw "conditiondialog.ui is not valid XML: $($_.Exception.Message)"
}

$namedObjects = @(
    $uiXml.SelectNodes(
        "//widget[@name] | //layout[@name] | //spacer[@name] | //action[@name]"
    )
)
$objectNames = @($namedObjects | ForEach-Object { [string]$_.name })
$duplicateNames = @(
    $objectNames |
        Group-Object |
        Where-Object Count -gt 1 |
        ForEach-Object Name
)
Assert-SourceCheck ($duplicateNames.Count -eq 0) `
    "Duplicate Qt object names: $($duplicateNames -join ', ')"

$requiredObjects = @(
    "comboVillageRotation",
    "comboBastionRotation",
    "comboIglooOrientation",
    "comboIglooSize",
    "comboAncientStart",
    "comboAncientRotation",
    "comboChambersStart",
    "comboChambersRotation",
    "comboTempleOrientation",
    "pageAncientCity",
    "pageChambers",
    "pageTemple"
)
$missingObjects = @($requiredObjects | Where-Object { $_ -notin $objectNames })
Assert-SourceCheck ($missingObjects.Count -eq 0) `
    "Required Qt objects are missing: $($missingObjects -join ', ')"

$stackedWidget = $uiXml.SelectSingleNode("//widget[@name='stackedWidget']")
Assert-SourceCheck ($null -ne $stackedWidget) "stackedWidget was not found."
$directPages = @($stackedWidget.SelectNodes("./widget") | ForEach-Object { [string]$_.name })
$requiredPages = @("pageAncientCity", "pageChambers", "pageTemple")
$nestedPages = @($requiredPages | Where-Object { $_ -notin $directPages })
Assert-SourceCheck ($nestedPages.Count -eq 0) `
    "Pages must be direct children of stackedWidget: $($nestedPages -join ', ')"

$dialogText = Get-Content -LiteralPath $dialogPath -Raw -Encoding UTF8
$uiReferences = @(
    [regex]::Matches($dialogText, "ui->([A-Za-z_][A-Za-z0-9_]*)") |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne "setupUi" } |
        Sort-Object -Unique
)
$missingReferences = @($uiReferences | Where-Object { $_ -notin $objectNames })
Assert-SourceCheck ($missingReferences.Count -eq 0) `
    "conditiondialog.cpp refers to missing Qt objects: $($missingReferences -join ', ')"

$searchText = Get-Content -LiteralPath $searchPath -Raw -Encoding UTF8
$headerText = Get-Content -LiteralPath $headerPath -Raw -Encoding UTF8
$variantDependencies = @(
    "DEP_VILLAGE_ROTATION",
    "DEP_BASTION_ROTATION",
    "DEP_IGLOO_ORIENTATION",
    "DEP_IGLOO_SIZE",
    "DEP_ANCIENT_START",
    "DEP_ANCIENT_ROTATION",
    "DEP_CHAMBERS_START",
    "DEP_CHAMBERS_ROTATION",
    "DEP_TEMPLE_ORIENTATION"
)
foreach ($dependency in $variantDependencies) {
    $count = [regex]::Matches(
        "$headerText`n$searchText`n$dialogText",
        "\b$dependency\b"
    ).Count
    Assert-SourceCheck ($count -eq 4) `
        "$dependency should have one declaration and three uses; found $count."
}

$scriptsText = Get-Content -LiteralPath $scriptsPath -Raw -Encoding UTF8
$lootHeaderText = Get-Content -LiteralPath $lootHeaderPath -Raw -Encoding UTF8
$lootSourceText = Get-Content -LiteralPath $lootSourcePath -Raw -Encoding UTF8
$lootConditionHeaderText = Get-Content -LiteralPath $lootConditionHeaderPath -Raw -Encoding UTF8
$lootConditionSourceText = Get-Content -LiteralPath $lootConditionSourcePath -Raw -Encoding UTF8
$lootWidgetHeaderText = Get-Content -LiteralPath $lootWidgetHeaderPath -Raw -Encoding UTF8
$lootWidgetSourceText = Get-Content -LiteralPath $lootWidgetSourcePath -Raw -Encoding UTF8
$villageStructureSourceText = Get-Content -LiteralPath $villageStructureSourcePath -Raw -Encoding UTF8
$villageLootSeedSourceText = Get-Content -LiteralPath $villageLootSeedSourcePath -Raw -Encoding UTF8
$cubiomesMakefileText = Get-Content -LiteralPath $cubiomesMakefilePath -Raw -Encoding UTF8
$cubiomesCmakeText = Get-Content -LiteralPath $cubiomesCmakePath -Raw -Encoding UTF8
$projectText = Get-Content -LiteralPath $projectPath -Raw -Encoding UTF8
$versionTestsText = Get-Content -LiteralPath $versionTestsPath -Raw -Encoding UTF8
$lootTestsText = Get-Content -LiteralPath $lootTestsPath -Raw -Encoding UTF8
$noticesText = Get-Content -LiteralPath $noticesPath -Raw -Encoding UTF8
$formSearchUiText = Get-Content -LiteralPath $formSearchUiPath -Raw -Encoding UTF8
$formSearchSourceText = Get-Content -LiteralPath $formSearchSourcePath -Raw -Encoding UTF8
$configSourceText = Get-Content -LiteralPath $configSourcePath -Raw -Encoding UTF8
$buildScriptText = Get-Content -LiteralPath $buildScriptPath -Raw -Encoding UTF8
$lootTestScriptText = Get-Content -LiteralPath $lootTestScriptPath -Raw -Encoding UTF8

Assert-SourceCheck ($scriptsText.Contains(
    'lua_setglobal(L, "getDesertPyramidLoot")'
)) "The desert-pyramid loot function is not registered in Lua."
Assert-SourceCheck ($scriptsText.Contains(
    'rules.append(Rule("\\b" "getDesertPyramidLoot" "\\b", format));'
)) "The desert-pyramid loot Lua function is missing syntax highlighting."
Assert-SourceCheck ($lootHeaderText.Contains("getDesertPyramidLoot16")) `
    "cubiomes/loot.h is missing the public desert-pyramid API."
foreach ($api in @(
    "getBuriedTreasureLoot16",
    "getRuinedPortalLoot16",
    "getShipwreckLoot16"
)) {
    Assert-SourceCheck ($lootHeaderText.Contains($api)) `
        "cubiomes/loot.h is missing $api."
}
Assert-SourceCheck ($lootHeaderText.Contains("desertPyramidEnchantmentName")) `
    "cubiomes/loot.h is missing the enchantment API."
Assert-SourceCheck ($lootSourceText.Contains("DP_DECORATION_SALT_16 = 40003")) `
    "cubiomes/loot.c is missing the Java 1.16 decoration salt."
Assert-SourceCheck ($lootSourceText.Contains("BURIED_DECORATION_SALT_16 = 30001")) `
    "cubiomes/loot.c is missing the buried-treasure decoration salt."
Assert-SourceCheck ($lootSourceText.Contains("PORTAL_DECORATION_SALT_16 = 40005")) `
    "cubiomes/loot.c is missing the ruined-portal decoration salt."
Assert-SourceCheck ($lootSourceText.Contains("SHIPWRECK_DECORATION_SALT_16 = 40006")) `
    "cubiomes/loot.c is missing the shipwreck decoration salt."
Assert-SourceCheck ($lootSourceText.Contains("out->enchantedBook")) `
    "cubiomes/loot.c is missing enchanted-book details."
Assert-SourceCheck ($lootConditionHeaderText.Contains("struct LootRuleSet")) `
    "src/lootcondition.h is missing the GUI loot-rule model."
Assert-SourceCheck ($lootConditionSourceText.Contains("matchAreaLoot")) `
    "src/lootcondition.cpp is missing area-total evaluation."
Assert-SourceCheck ($lootConditionSourceText.Contains("LootAccumulator")) `
    "src/lootcondition.cpp is missing wide-count aggregation."
Assert-SourceCheck ($lootWidgetHeaderText.Contains("class LootRuleEditor")) `
    "src/lootconditionwidget.h is missing the GUI editor."
Assert-SourceCheck ($lootWidgetSourceText.Contains("LootRuleEditor::addRule")) `
    "src/lootconditionwidget.cpp is missing dynamic rule rows."
Assert-SourceCheck ($dialogText.Contains("structureLootEditor")) `
    "conditiondialog.cpp is missing structure loot controls."
Assert-SourceCheck ($dialogText.Contains("areaLootEditor")) `
    "conditiondialog.cpp is missing Other/area-total loot controls."
Assert-SourceCheck ($dialogText.Contains("portalLootEditor")) `
    "conditiondialog.cpp is missing ruined-portal loot controls."
Assert-SourceCheck ($dialogText.Contains("simpleLootEditor")) `
    "conditiondialog.cpp is missing shipwreck/buried-treasure loot controls."
Assert-SourceCheck ($searchText.Contains("case F_LOOT")) `
    "src/search.cpp is missing the Other loot condition."
Assert-SourceCheck ($searchText.Contains("matchAreaLoot")) `
    "src/search.cpp is missing area-total search integration."
Assert-SourceCheck ($lootConditionSourceText.Contains("LootSearchCacheEntry")) `
    "src/lootcondition.cpp is missing the 48-bit Loot cache."
Assert-SourceCheck ($lootConditionSourceText.Contains("canMatchStructureLoot48")) `
    "src/lootcondition.cpp is missing the conservative 48-bit Loot precheck."
Assert-SourceCheck ($villageStructureSourceText.Contains("generateVillageLayout16")) `
    "src/villagestructure.cpp is missing exact Java 1.16.1 Village layout generation."
Assert-SourceCheck ($villageLootSeedSourceText.Contains("assignVillageLootSeedsSingleStart16")) `
    "src/villagelootseed.cpp is missing Village LootTableSeed assignment."
Assert-SourceCheck ($searchText.Contains("env->fastFamilyLoot")) `
    "src/search.cpp is missing the optional 48-bit Loot precheck wiring."
Assert-SourceCheck ($formSearchUiText.Contains('name="comboLootMode"')) `
    "formsearchcontrol.ui is missing the Loot search-mode selector."
Assert-SourceCheck ($formSearchSourceText.Contains('tr("Exact Loot search")')) `
    "The exhaustive Loot search mode is missing."
Assert-SourceCheck ($formSearchSourceText.Contains('tr("Fast sampled Loot search")')) `
    "The sampled Loot search mode is missing."
Assert-SourceCheck ($formSearchSourceText.Contains("fastFamilyLoot")) `
    "formsearchcontrol.cpp is missing the Loot speed-search setting."
Assert-SourceCheck ([regex]::IsMatch(
    $formSearchSourceText,
    "supportsFast48Loot[\s\S]{0,160}SEARCH_BLOCKS\s*\|\|" +
        "\s*searchType\s*==\s*SEARCH_48ONLY"
)) "The Loot speed-search option is not enabled for 48-bit only."
Assert-SourceCheck ($configSourceText.Contains("#FastLoot48:")) `
    "SearchConfig does not save the Loot speed-search setting."
Assert-SourceCheck ($lootTestScriptText.Contains("Test-Headless48Session")) `
    "The 48-bit-only Loot integration regression is missing."
Assert-SourceCheck ($buildScriptText.Contains("windeployqt.exe")) `
    "dev-build.ps1 does not deploy the Qt runtime for direct EXE startup."
Assert-SourceCheck ($cubiomesMakefileText.Contains("loot.c")) `
    "cubiomes/makefile does not compile loot.c."
Assert-SourceCheck ($cubiomesCmakeText.Contains("loot.c")) `
    "cubiomes/CMakeLists.txt does not compile loot.c."
Assert-SourceCheck ($projectText.Contains('$$CUPATH/loot.h')) `
    "seed-quarry.pro does not track cubiomes/loot.h."
Assert-SourceCheck ($projectText.Contains('src/lootcondition.cpp')) `
    "seed-quarry.pro does not compile src/lootcondition.cpp."
Assert-SourceCheck ($projectText.Contains('src/lootconditionwidget.cpp')) `
    "seed-quarry.pro does not compile src/lootconditionwidget.cpp."
Assert-SourceCheck ($projectText.Contains('src/villagestructure.cpp')) `
    "seed-quarry.pro does not compile src/villagestructure.cpp."
Assert-SourceCheck ($projectText.Contains('src/villagelootseed.cpp')) `
    "seed-quarry.pro does not compile src/villagelootseed.cpp."
Assert-SourceCheck ($versionTestsText.Contains("3515201313347228787ULL")) `
    "The MineMap desert-pyramid golden-vector test is missing."
Assert-SourceCheck ($lootTestsText.Contains("widePositions.fill(pyramid, 70000)")) `
    "The wide area-total regression test is missing."
Assert-SourceCheck ($lootTestsText.Contains("SEM_NOGPFAULTERRORBOX")) `
    "Loot tests may still open a modal Windows crash dialog."
Assert-SourceCheck ($noticesText.Contains("SeedFinding Java libraries")) `
    "The SeedFinding attribution is missing from THIRD_PARTY_NOTICES.md."

foreach ($scriptPath in @(
    $buildScriptPath,
    $lootTestScriptPath,
    $shortcutScriptPath,
    $rebuildRunScriptPath
)) {
    $tokens = $null
    $parseErrors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        $scriptPath,
        [ref]$tokens,
        [ref]$parseErrors
    ) | Out-Null
    Assert-SourceCheck ($parseErrors.Count -eq 0) `
        "$([System.IO.Path]::GetFileName($scriptPath)) has PowerShell syntax errors: $($parseErrors.Message -join '; ')"
}

Write-Host "Source checks passed:"
Write-Host "  UTF-8 and conditiondialog.ui XML"
Write-Host "  unique Qt object names and C++ UI references"
Write-Host "  structure-variant dependency wiring"
Write-Host "  1.16 structure loot source, GUI, search, Lua API, tests, and attribution"
Write-Host "  build, test, and shortcut PowerShell syntax"
