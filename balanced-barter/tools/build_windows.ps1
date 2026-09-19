[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
$CommonCommit = 'b93280e832f263dbef44e44cbe2936622a02f91a'
$VcpkgCommit = '60b06921c7c7ac787b23a222dfab5cdd3911712e'
$SkyCommit = '3d537a366f1a45c14a7012ea0aa00be18108f073'
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
$Sky = Join-Path $WorkDirectory 'skyui'
$Build = Join-Path $WorkDirectory 'build'
$Stage = Join-Path $WorkDirectory 'stage'
ClonePinned 'https://github.com/doodlum/SkyUI-Community.git' $Sky $SkyCommit
Run 'python' @((Join-Path $Root 'tools/build_ui.py'), $Sky, (Join-Path $WorkDirectory 'ui'), (Join-Path $Stage 'Interface/bartermenu.swf'))
ClonePinned 'https://github.com/CharmedBaryon/CommonLibSSE-NG.git' $Common $CommonCommit
ClonePinned 'https://github.com/microsoft/vcpkg.git' $Vcpkg $VcpkgCommit
Run (Join-Path $Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
Run 'cmake' @('-S', $Root, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_TOOLCHAIN_FILE=$Vcpkg/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md', "-DCOMMONLIB_ROOT=$Common")
$VSWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$MSBuild = (& $VSWhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1)
if (!$MSBuild) { throw 'MSBuild was not found' }
Run $MSBuild @((Join-Path $Build 'BalancedBarter.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--target', 'trade_tests', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--parallel', '2')
$PluginDirectory = Join-Path $Stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force -Path $PluginDirectory | Out-Null
$Dll = Join-Path $Build 'Release/BalancedBarter.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
Copy-Item $Dll $PluginDirectory
Copy-Item (Join-Path $Root 'README.md') $Stage
Copy-Item (Join-Path $Root 'LICENSE') $Stage
Copy-Item (Join-Path $Root 'UPSTREAM-NOTICES.md') $Stage
$Licenses = Join-Path $Stage 'licenses'
New-Item -ItemType Directory -Force -Path $Licenses | Out-Null
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Licenses 'CommonLibSSE-LICENSE')
foreach ($Port in @('fmt','spdlog','rapidcsv')) {
    $Copyright = Join-Path $Build "vcpkg_installed/x64-windows-static-md/share/$Port/copyright"
    if (!(Test-Path $Copyright)) { throw "Missing dependency license: $Port" }
    Copy-Item $Copyright (Join-Path $Licenses "$Port-LICENSE")
}
$Sources = [ordered]@{}
foreach ($Name in @('src/main.cpp','src/TradeRules.h','tests/trade_tests.cpp','ui/BarterExtensions.as','tools/build_ui.py','tools/build_windows.ps1','CMakeLists.txt','vcpkg.json','README.md','UPSTREAM-NOTICES.md')) {
    $Text = [IO.File]::ReadAllText((Join-Path $Root $Name)).Replace("`r`n", "`n")
    $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $Sources[$Name] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
$Info = [ordered]@{
    plugin = 'BalancedBarter'; version = '0.1.0'; runtime = '1.6.1170'; skyui = '6.11'
    source_commit = $env:GITHUB_SHA; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit; skyui_commit = $SkyCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    swf_sha256 = (Get-FileHash (Join-Path $Stage 'Interface/bartermenu.swf') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources; windows_build = 'passed'; settlement_and_rollback_tests = 'passed'; ui_compilation = 'passed'; in_game_tested = $false
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Balanced_Barter_SkyUI6_v0_1.zip')
