[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
$CommonCommit = 'b93280e832f263dbef44e44cbe2936622a02f91a'
$VcpkgCommit = '60b06921c7c7ac787b23a222dfab5cdd3911712e'
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
function ClonePinned([string]$Url, [string]$Destination, [string]$Commit) {
    if (Test-Path $Destination) { throw "Directory exists: $Destination" }
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
ClonePinned 'https://github.com/CharmedBaryon/CommonLibSSE-NG.git' $Common $CommonCommit
ClonePinned 'https://github.com/microsoft/vcpkg.git' $Vcpkg $VcpkgCommit
Run (Join-Path $Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
Run 'cmake' @('-S', $Root, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_TOOLCHAIN_FILE=$Vcpkg/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',
    "-DCOMMONLIB_ROOT=$Common")
# Compile the adapter first: catch API/template errors before building CommonLib.
$VSWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$MSBuild = (& $VSWhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1)
if (!$MSBuild -or !(Test-Path $MSBuild)) { throw 'MSBuild was not found by vswhere' }
Run $MSBuild @((Join-Path $Build 'RunemasterEnchantmentBridge.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
# Run independent rules and ABI checks before the full support-library build.
Run 'cmake' @('--build', $Build, '--config', 'Release', '--target', 'rules_tests', 'crafting_tests', 'abi_tests', 'visual_tests', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
# Copy only this DLL: CommonLib's install rules are not part of the mod package.
$PluginDirectory = Join-Path $Stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force -Path $PluginDirectory | Out-Null
$Dll = Join-Path $Build 'Release/RunemasterEnchantmentBridge.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
Copy-Item $Dll $PluginDirectory
Run 'python' @((Join-Path $Root 'tools/build_plugin.py'), (Join-Path $Stage 'Runemaster Enchanted Weapons.esp'))
Copy-Item (Join-Path $Root 'README.md') $Stage
Copy-Item (Join-Path $Root 'LICENSE') $Stage
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Stage 'CommonLibSSE-LICENSE')
$Sources = [ordered]@{}
foreach ($Name in @('src/main.cpp','src/Rules.h','src/Crafting.cpp','src/Crafting.h','src/CraftingRules.h','src/Visuals.cpp','src/Visuals.h','src/VisualRules.h','tests/rules_tests.cpp','tests/crafting_tests.cpp','tests/abi_tests.cpp','tests/visual_tests.cpp','tools/build_plugin.py','CMakeLists.txt','vcpkg.json')) {
    # Normalize checkout CRLF for reproducible source identification.
    $Text = [IO.File]::ReadAllText((Join-Path $Root $Name)).Replace("`r`n", "`n")
    $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $Sources[$Name] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
$Info = [ordered]@{
    plugin = 'RunemasterEnchantmentBridge'; version = '1.2.0'; runtime = '1.6.1170'
    source_commit = $env:GITHUB_SHA; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources; windows_build = 'passed'; rules_tests = 'passed'; in_game_tested = $false
    runemaster_version = '1.5'; no_new_inventory_items = $true; permanent_weapons = 8; crafting_preservation_tests = 'passed'; inventory_hook_abi_tests = 'passed'; visual_lifecycle_tests = 'passed'; independent_rune_visuals = $true
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Runemaster_Enchanted_Weapons_v1_2.zip')
