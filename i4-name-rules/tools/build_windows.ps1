[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
$I4Commit = '52cc4cb30f2ec09eba104606c8cdd17050c3a2bf'
$CommonCommit = '25a9d8dca44979ae756ef13f1a8bed35bd83b414'
$CMakeCommit = 'e559058ed5908224fb12ea9b64d3206e636228ec'
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
$I4 = Join-Path $WorkDirectory 'InventoryInjector'
$Vcpkg = Join-Path $WorkDirectory 'vcpkg'
$Build = Join-Path $WorkDirectory 'build'
$Stage = Join-Path $WorkDirectory 'stage'
ClonePinned 'https://github.com/Exit-9B/InventoryInjector.git' $I4 $I4Commit
Run 'git' @('-C', $I4, 'submodule', 'update', '--init', '--recursive')
if ((& git -C (Join-Path $I4 'external/CommonLibSSE') rev-parse HEAD).Trim() -ne $CommonCommit) { throw 'CommonLib revision mismatch' }
if ((& git -C (Join-Path $I4 'tools/SKSE-CMakeModules') rev-parse HEAD).Trim() -ne $CMakeCommit) { throw 'CMake module revision mismatch' }
Run 'git' @('-C', $I4, 'apply', '--check', (Join-Path $Root 'name-keywords.patch'))
Run 'git' @('-C', $I4, 'apply', (Join-Path $Root 'name-keywords.patch'))
Run 'cmake' @('-S', (Join-Path $Root 'tests'), '-B', (Join-Path $WorkDirectory 'tests'),
    '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DI4_SOURCE=$I4")
Run 'cmake' @('--build', (Join-Path $WorkDirectory 'tests'), '--config', 'Release')
Run 'ctest' @('--test-dir', (Join-Path $WorkDirectory 'tests'), '-C', 'Release', '--output-on-failure')
ClonePinned 'https://github.com/microsoft/vcpkg.git' $Vcpkg $VcpkgCommit
Run (Join-Path $Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
$env:VCPKG_ROOT = $Vcpkg
Run 'cmake' @('-S', $I4, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_TOOLCHAIN_FILE=$Vcpkg/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',
    '-DSKSE_NO_INSTALL=ON', '-DBUILD_SKYRIMVR=OFF', '-DCMAKE_CXX_FLAGS=/EHsc /MP2 /W0')
$VSWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$MSBuild = (& $VSWhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1)
if (!$MSBuild) { throw 'MSBuild was not found' }
Run $MSBuild @((Join-Path $Build 'InventoryInjector.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--target', 'InventoryInjector', '--parallel', '2')
$Dll = Join-Path $Build 'Release/InventoryInjector.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
New-Item -ItemType Directory -Force -Path (Join-Path $Stage 'SKSE/Plugins') | Out-Null
Copy-Item $Dll (Join-Path $Stage 'SKSE/Plugins')
$Licenses = Join-Path $Stage 'Licenses/InventoryInjector-Name-Keywords'
New-Item -ItemType Directory -Force -Path $Licenses | Out-Null
Copy-Item (Join-Path $I4 'LICENSE') (Join-Path $Licenses 'InventoryInjector-LICENSE')
Copy-Item (Join-Path $I4 'external/CommonLibSSE/LICENSE') (Join-Path $Licenses 'CommonLibSSE-LICENSE')
Get-ChildItem (Join-Path $Build 'vcpkg_installed/x64-windows-static-md/share') -Directory | ForEach-Object {
    $Copyright = Join-Path $_.FullName 'copyright'
    if (Test-Path $Copyright) { Copy-Item $Copyright (Join-Path $Licenses ($_.Name + '-LICENSE')) }
}
$Source = Join-Path $Stage 'Source/InventoryInjector-Name-Keywords'
New-Item -ItemType Directory -Force -Path (Join-Path $Source 'InventoryInjector') | Out-Null
foreach ($Name in @('src','docs','CMakeLists.txt','CMakePresets.json','vcpkg.json','vcpkg-configuration.json','.clang-format','.editorconfig','.gitmodules','LICENSE','README.md')) {
    Copy-Item (Join-Path $I4 $Name) (Join-Path $Source 'InventoryInjector') -Recurse
}
foreach ($Name in @('tools','tests','name-keywords.patch','README.md')) {
    Copy-Item (Join-Path $Root $Name) $Source -Recurse
}
$Info = [ordered]@{
    patchVersion = '1.3'; component = 'InventoryInjector name keyword extension'; upstreamVersion = '1.1.1'
    builtUTC = [DateTime]::UtcNow.ToString('o'); i4Commit = $I4Commit; commonLibCommit = $CommonCommit
    cmakeModulesCommit = $CMakeCommit; vcpkgToolingCommit = $VcpkgCommit
    dependencyRegistryCommit = 'a7b6122f6b6504d16d96117336a0562693579933'; patchCommit = $env:GITHUB_SHA
    targetRuntime = 'Steam Skyrim SE 1.6.1170'; dllSHA256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLower()
    patchSHA256 = (Get-FileHash (Join-Path $Root 'name-keywords.patch') -Algorithm SHA256).Hash.ToLower()
    automatedTests = '22 name-matching checks'; inGameTested = $false
}
$Info | ConvertTo-Json | Set-Content (Join-Path $Stage 'I4-KEYWORD-BUILD-INFO.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'InventoryInjector_Name_Keywords_v1_3.zip') -CompressionLevel Optimal
