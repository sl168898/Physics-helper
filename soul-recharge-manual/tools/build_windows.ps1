[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
$CommonCommit = 'b93280e832f263dbef44e44cbe2936622a02f91a'
$VcpkgCommit = '60b06921c7c7ac787b23a222dfab5cdd3911712e'
$ApiCommit = '1dcb70179076aae4ab626f43c5baab2735ca5877'
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
ClonePinned 'https://github.com/CharmedBaryon/CommonLibSSE-NG.git' $Common $CommonCommit
ClonePinned 'https://github.com/microsoft/vcpkg.git' $Vcpkg $VcpkgCommit
Run (Join-Path $Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
Run 'cmake' @('-S', $Root, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_TOOLCHAIN_FILE=$Vcpkg/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md', "-DCOMMONLIB_ROOT=$Common")
$VSWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$MSBuild = (& $VSWhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1)
if (!$MSBuild) { throw 'MSBuild was not found' }
Run $MSBuild @((Join-Path $Build 'SoulRechargeMenu.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--target', 'core_tests', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--parallel', '2')
Copy-Item (Join-Path $Root 'original') $Stage -Recurse -Force
$Docs = Join-Path $Stage 'Docs'
New-Item -ItemType Directory -Force -Path $Docs | Out-Null
Move-Item (Join-Path $Stage 'README.md') (Join-Path $Docs 'Original-README.md')
Copy-Item (Join-Path $Root 'README.md') $Stage
Copy-Item (Join-Path $Root 'BRIDGE-NOTES.md') $Stage
Copy-Item (Join-Path $Root 'Scripts/AGH_SR_MCM.pex') (Join-Path $Stage 'Scripts/AGH_SR_MCM.pex') -Force
$Dll = Join-Path $Build 'Release/SoulRechargeMenu.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
Copy-Item $Dll (Join-Path $Stage 'SKSE/Plugins')
$Licenses = Join-Path $Stage 'licenses'
New-Item -ItemType Directory -Force -Path $Licenses | Out-Null
Copy-Item (Join-Path $Root 'ADDON-LICENSE') (Join-Path $Licenses 'SoulRechargeMenu-MIT.txt')
Copy-Item (Join-Path $Root 'vendor/SKSEMenuFramework-LICENSE') (Join-Path $Licenses 'SKSEMenuFramework-API-LGPL-2.1.txt')
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Licenses 'CommonLibSSE-LICENSE')
foreach ($Port in @('fmt','spdlog','rapidcsv','nlohmann-json')) {
    $Copyright = Join-Path $Build "vcpkg_installed/x64-windows-static-md/share/$Port/copyright"
    if (!(Test-Path $Copyright)) { throw "Missing dependency license: $Port" }
    Copy-Item $Copyright (Join-Path $Licenses "$Port-LICENSE")
}
$SourceDirectory = Join-Path $Stage 'Source/SoulRechargeMenu'
New-Item -ItemType Directory -Force -Path $SourceDirectory | Out-Null
foreach ($Name in @('src','tests','tools','vendor','headers','CMakeLists.txt','vcpkg.json','ADDON-LICENSE','BRIDGE-NOTES.md')) {
    Copy-Item (Join-Path $Root $Name) $SourceDirectory -Recurse
}
New-Item -ItemType Directory -Force -Path (Join-Path $SourceDirectory 'Scripts/Source') | Out-Null
Copy-Item (Join-Path $Root 'Scripts/Source/AGH_SR_MCM.psc') (Join-Path $SourceDirectory 'Scripts/Source')
$Sources = [ordered]@{}
foreach ($Name in @('src/main.cpp','src/CoreBridge.h','src/InputRules.h','tests/core_tests.cpp','tools/build_windows.ps1','CMakeLists.txt','vcpkg.json','Scripts/Source/AGH_SR_MCM.psc','README.md','BRIDGE-NOTES.md')) {
    $Text = [IO.File]::ReadAllText((Join-Path $Root $Name)).Replace("`r`n", "`n")
    $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $Sources[$Name] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
$Info = [ordered]@{
    plugin = 'SoulRechargeMenu'; version = '1.2.0'; runtime = '1.6.1170'; original_core = '1.1.0'
    source_commit = $env:GITHUB_SHA; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit; menu_api_commit = $ApiCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    original_dll_sha256 = (Get-FileHash (Join-Path $Stage 'SKSE/Plugins/AGH_SoulRecharge_G0.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    mcm_retirement_pex_sha256 = (Get-FileHash (Join-Path $Stage 'Scripts/AGH_SR_MCM.pex') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources; windows_build = 'passed'; original_kernel_and_hotkey_tests = 'passed'; in_game_tested = $false
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Soul_Gem_Recharge_Manual_Menu_v1_2.zip')
