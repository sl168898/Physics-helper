[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))
$ErrorActionPreference = 'Stop'
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
foreach ($Name in @('README.md','TESTING.md','DESIGN.md','LICENSE')) { Copy-Item (Join-Path $Root $Name) $Stage }
$Licenses = Join-Path $Stage 'Licenses'
New-Item -ItemType Directory -Force -Path $Licenses | Out-Null
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Licenses 'CommonLibSSE-LICENSE')
Get-ChildItem (Join-Path $Build 'vcpkg_installed/x64-windows-static-md/share') -Directory | ForEach-Object {
    $Copyright = Join-Path $_.FullName 'copyright'
    if (Test-Path $Copyright) { Copy-Item $Copyright (Join-Path $Licenses ($_.Name + '-LICENSE')) }
}
$Source = Join-Path $Stage 'Source'
New-Item -ItemType Directory -Force -Path $Source | Out-Null
foreach ($Name in @('src','tests','tools','Data','CMakeLists.txt','vcpkg.json','README.md','TESTING.md','DESIGN.md','LICENSE')) {
    Copy-Item (Join-Path $Root $Name) $Source -Recurse
}
$Sources = [ordered]@{}
Get-ChildItem $Source -Recurse -File | Sort-Object FullName | ForEach-Object {
    $Relative = [IO.Path]::GetRelativePath($Source, $_.FullName).Replace('\','/')
    $Text = [IO.File]::ReadAllText($_.FullName).Replace("`r`n", "`n")
    $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $Sources[$Relative] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
$Info = [ordered]@{
    plugin = 'PoisonedAmmoNative'; version = '0.2.0-beta'; runtime = 'Steam 1.6.1170'
    source_commit = $env:GITHUB_SHA; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    esp_sha256 = (Get-FileHash (Join-Path $Stage 'PoisonedAmmoNative.esp') -Algorithm SHA256).Hash.ToLowerInvariant()
    perks_sha256 = (Get-FileHash (Join-Path $Stage 'CoatingMechanist.esp') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources
    windows_build = 'passed'; portable_tests = 'passed'; esp_structure_validation = 'passed'
    in_game_tested = $false; recipe_capacity = 512; dynamic_form_creation = $false; papyrus_scripts = $false
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Poisoned_Ammunition_Coating_Perks_v0_2_0_Beta.zip')
