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
Run $MSBuild @((Join-Path $Build 'WhitePhialNames.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
# Copy only this DLL: CommonLib's install rules are not part of the mod package.
$PluginDirectory = Join-Path $Stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force -Path $PluginDirectory | Out-Null
$Dll = Join-Path $Build 'Release/WhitePhialNames.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
Copy-Item $Dll $PluginDirectory
Copy-Item (Join-Path $Root 'README.md') $Stage
Copy-Item (Join-Path $Root 'LICENSE') $Stage
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Stage 'CommonLibSSE-LICENSE')
Run 'python' @((Join-Path $Root 'tools/stage_data.py'), $Stage)
$SourceStage = Join-Path $Stage 'Source/WhitePhialNames'
New-Item -ItemType Directory -Force -Path $SourceStage | Out-Null
foreach ($Name in @('src','tests','tools','data','papyrus','CMakeLists.txt','vcpkg.json','README.md','LICENSE')) {
    Copy-Item (Join-Path $Root $Name) $SourceStage -Recurse
}
$Licenses = Join-Path $Stage 'Licenses'
New-Item -ItemType Directory -Force -Path $Licenses | Out-Null
foreach ($Dependency in @('spdlog','fmt','rapidcsv')) {
    Copy-Item (Join-Path $Build "vcpkg_installed/x64-windows-static-md/share/$Dependency/copyright") (Join-Path $Licenses "$Dependency.txt")
}
$Sources = [ordered]@{}
foreach ($Name in @('src/main.cpp','src/Names.h','src/Bank.h','src/Storage.cpp','src/Storage.h','tests/names_tests.cpp','tests/bank_tests.cpp','CMakeLists.txt','vcpkg.json','tools/build_windows.ps1','tools/build_plugin.py','tools/stage_data.py','tools/esp.py','README.md','data/PapyrusBuild.json','papyrus/WPD_Names.psc','papyrus/WPD_Storage.psc','papyrus/WPD_DecantQuest.psc','papyrus/WPD_PlayerAlias.psc','papyrus/TWPTE_WhitePhialActorScript.psc')) {
    # Normalize checkout CRLF for reproducible source identification.
    $Text = [IO.File]::ReadAllText((Join-Path $Root $Name)).Replace("`r`n", "`n")
    $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $Sources[$Name] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
$Info = [ordered]@{
    plugin = 'WhitePhialNames'; version = '2.0.0'; addon_version = '2.0 beta'; runtime = '1.6.1170'
    source_commit = $env:GITHUB_SHA; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources; windows_build = 'passed'; name_tests = 'passed'; bank_tests = 'passed'; in_game_tested = $false
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'White_Phial_Decanting_v2_0_beta.zip')
