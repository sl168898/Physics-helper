[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))
$ErrorActionPreference = 'Stop'
$env:PYTHONDONTWRITEBYTECODE = '1'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
$CommonCommit = 'b93280e832f263dbef44e44cbe2936622a02f91a'
$VcpkgCommit = '382c5b8a94b3d6b6286df7a488c7efa8d37313eb'
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
function ClonePinned([string]$Url, [string]$Destination, [string]$Commit) {
    Run 'git' @('clone', '--no-checkout', $Url, $Destination)
    Run 'git' @('-C', $Destination, 'checkout', '--detach', $Commit)
    $Actual = (& git -C $Destination rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $Actual -ne $Commit) { throw 'Revision mismatch' }
}
New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
$Common = Join-Path $WorkDirectory 'commonlib'
$Vcpkg = Join-Path $WorkDirectory 'vcpkg'
$Build = Join-Path $WorkDirectory 'build'
$Stage = Join-Path $WorkDirectory 'stage'
Run 'cmake' @('-S', (Join-Path $Root 'tests'), '-B', (Join-Path $WorkDirectory 'tests'), '-G', 'Visual Studio 17 2022', '-A', 'x64')
Run 'cmake' @('--build', (Join-Path $WorkDirectory 'tests'), '--config', 'Release')
Run 'ctest' @('--test-dir', (Join-Path $WorkDirectory 'tests'), '-C', 'Release', '--output-on-failure')
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
Run 'python' @((Join-Path $Root 'tools/make_plugin.py'), (Join-Path $Stage 'PoisonedAmmoNative.esp'))
Run 'python' @((Join-Path $Root 'tools/make_coating_perks.py'), (Join-Path $Stage 'CoatingMechanist.esp'))
ClonePinned 'https://github.com/CharmedBaryon/CommonLibSSE-NG.git' $Common $CommonCommit
ClonePinned 'https://github.com/microsoft/vcpkg.git' $Vcpkg $VcpkgCommit
Run (Join-Path $Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
$env:VCPKG_ROOT = $Vcpkg
Run 'cmake' @('-S', $Root, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_TOOLCHAIN_FILE=$Vcpkg/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',
    "-DCOMMONLIB_ROOT=$Common", '-DCMAKE_CXX_FLAGS=/EHsc /MP2')
$VSWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$MSBuild = (& $VSWhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1)
if (!$MSBuild) { throw 'MSBuild was not found' }
# Catch native API errors before compiling the entire engine binding library.
Run $MSBuild @((Join-Path $Build 'PoisonedAmmoNative.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
$Dll = Join-Path $Build 'Release/PoisonedAmmoNative.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
$PluginDirectory = Join-Path $Stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force -Path $PluginDirectory | Out-Null
Copy-Item $Dll $PluginDirectory
Copy-Item (Join-Path $Root 'Data/SKSE/Plugins/*') $PluginDirectory -Recurse
foreach ($Name in @('README.md','TESTING.md','DESIGN.md','CRASH_FIX.md','LICENSE')) { Copy-Item (Join-Path $Root $Name) $Stage }
$Licenses = Join-Path $Stage 'Licenses'
New-Item -ItemType Directory -Force -Path $Licenses | Out-Null
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Licenses 'CommonLibSSE-LICENSE')
Get-ChildItem (Join-Path $Build 'vcpkg_installed/x64-windows-static-md/share') -Directory | ForEach-Object {
    $Copyright = Join-Path $_.FullName 'copyright'
    if (Test-Path $Copyright) { Copy-Item $Copyright (Join-Path $Licenses ($_.Name + '-LICENSE')) }
}
# Source is versioned in Git. Ship an exact commit pointer and LF-normalized
# source hashes instead of duplicating the repository in the install archive.
$Sources = [ordered]@{}
$SourcePaths = @('src','tests','tools','Data','CMakeLists.txt','vcpkg.json','README.md','TESTING.md','DESIGN.md','CRASH_FIX.md','LICENSE')
foreach ($Name in $SourcePaths) {
    $SourcePath = Join-Path $Root $Name
    $SourceFiles = if (Test-Path $SourcePath -PathType Container) {
        Get-ChildItem -LiteralPath $SourcePath -Recurse -File
    } else {
        Get-Item -LiteralPath $SourcePath
    }
    $SourceFiles | Sort-Object FullName | ForEach-Object {
        $Relative = [IO.Path]::GetRelativePath($Root, $_.FullName).Replace('\','/')
        $Text = [IO.File]::ReadAllText($_.FullName).Replace("`r`n", "`n")
        $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
        $Sources[$Relative] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
    }
}
$SourceUrl = "https://github.com/sl168898/Physics-helper/tree/$($env:GITHUB_SHA)/poisoned-ammo-skse"
"Exact build source: $SourceUrl`nBuild instructions: tools/build_windows.ps1`nSource file hashes: BuildInfo.json" | Set-Content (Join-Path $Stage 'SOURCE.txt') -Encoding utf8
$Info = [ordered]@{
    plugin = 'PoisonedAmmoNative'; version = '0.4.0-beta'; runtime = 'Steam 1.6.1170'
    source_commit = $env:GITHUB_SHA; source_url = $SourceUrl; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    esp_sha256 = (Get-FileHash (Join-Path $Stage 'PoisonedAmmoNative.esp') -Algorithm SHA256).Hash.ToLowerInvariant()
    perks_sha256 = (Get-FileHash (Join-Path $Stage 'CoatingMechanist.esp') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources
    windows_build = 'passed'; portable_tests = 'passed'; esp_structure_validation = 'passed'
    coating_strength_multipliers = @(1.0, 1.25, 1.5); rank_ii_replaces_rank_i = $true
    impact_pointer_abi_fixed = $true; production_impact_wrapper_test = 'passed'
    wheeler_icon_api = 'PoisonedAmmoNative_GetIconInfoV1'; icon_metadata_tests = 'passed'; icon_changes_only = $false
    wheeler_coat_api = 'PoisonedAmmoNative_CoatOneV1'; wheeler_runtime_verified = $false
    inventory_click_to_coat = $true; click_bottles = 1; click_dialog = $false; hotkey_batch_dialog = $true
    immersive_interactions_bridge = $true; animation_compatibility_verified = $false
    crossbow_reload_before_poison = $true; reload_sequence_tests = 'passed'
    crafted_poison_handle_check_fixed = $true; production_poison_snapshot_tests = 'passed'
    runtime_keyword_persistence = $true; runtime_keyword_tests = 'passed'; legacy_v1_golden_tests = 'passed'
    global_form_keyword_lookup = $true; factory_keyword_regression_tests = 'passed'
    alchemical_precision = $false; alchemical_potency = $true; potency_form_id = '0x803|CoatingMechanist.esp'
    potency_marksman = 60; potency_parent = 'Measured Dose'; potency_damage_per_alchemy_point = 0.01
    potency_skill_actor_value = 'Alchemy'; potency_uses_poison_strength_modifier = $false
    potency_damage_only = $true; potency_crossbow_bolts_only = $true; potency_production_hook_tests = 'passed'
    critical_hooks_installed = $false
    recipe_payload_versions = @(1, 2)
    in_game_tested = $false; recipe_capacity = 512; dynamic_form_creation = $false; papyrus_scripts = $false
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Poisoned_Ammunition_Coating_Perks_v0_4_0_Beta.zip')
