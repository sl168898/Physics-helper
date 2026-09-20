[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
$RefinedCommit = 'e3360bf81d05f739d2caa94d50e64e174b0ce1f8'
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
$Refined = Join-Path $WorkDirectory 'refined'
$Common = Join-Path $WorkDirectory 'commonlib'
$Vcpkg = Join-Path $WorkDirectory 'vcpkg'
$Build = Join-Path $WorkDirectory 'build'
$Stage = Join-Path $WorkDirectory 'stage'
ClonePinned 'https://github.com/c0kadam/Wheeler-Refined.git' $Refined $RefinedCommit
ClonePinned 'https://github.com/CharmedBaryon/CommonLibSSE-NG.git' $Common $CommonCommit
ClonePinned 'https://github.com/microsoft/vcpkg.git' $Vcpkg $VcpkgCommit
Run 'git' @('-C', $Refined, 'apply', '--check', (Join-Path $Root 'wheeler-compat.patch'))
Run 'git' @('-C', $Refined, 'apply', (Join-Path $Root 'wheeler-compat.patch'))
Run (Join-Path $Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
$env:VCPKG_ROOT = $Vcpkg
Run 'cmake' @('-S', $Refined, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_TOOLCHAIN_FILE=$Vcpkg/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',
    "-DCommonLibSSEPath_NG=$Common", '-DCOPY_OUTPUT=OFF', '-DCMAKE_CXX_FLAGS=/EHsc /MP2 /W0')
$VSWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$MSBuild = (& $VSWhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1)
if (!$MSBuild) { throw 'MSBuild was not found' }
# Compile the modified plugin first, before spending time compiling CommonLib.
Run $MSBuild @((Join-Path $Build 'src/wheeler.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('-S', (Join-Path $Root 'tests'), '-B', (Join-Path $WorkDirectory 'tests'),
    '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DREFINED_SOURCE=$Refined")
Run 'cmake' @('--build', (Join-Path $WorkDirectory 'tests'), '--config', 'Release')
Run 'ctest' @('--test-dir', (Join-Path $WorkDirectory 'tests'), '-C', 'Release', '--output-on-failure')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--target', 'wheeler', '--parallel', '2')
$Dll = Join-Path $Build 'src/Release/wheeler.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
New-Item -ItemType Directory -Force -Path (Join-Path $Stage 'SKSE/Plugins') | Out-Null
Copy-Item $Dll (Join-Path $Stage 'SKSE/Plugins')
Copy-Item (Join-Path $Root 'README.md') (Join-Path $Stage 'README-Wheeler-Compatibility.md')
$Licenses = Join-Path $Stage 'Licenses'
New-Item -ItemType Directory -Force -Path $Licenses | Out-Null
foreach ($Name in @('LICENSE','LICENSES','NOTICE.md','THIRD_PARTY_NOTICES.md')) {
    Copy-Item (Join-Path $Refined $Name) $Licenses -Recurse
}
Copy-Item (Join-Path $Refined 'Data/SKSE/Plugins/third-party-notices') $Licenses -Recurse
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Licenses 'CommonLibSSE-LICENSE')
Get-ChildItem (Join-Path $Build 'vcpkg_installed/x64-windows-static-md/share') -Directory | ForEach-Object {
    $Copyright = Join-Path $_.FullName 'copyright'
    if (Test-Path $Copyright) { Copy-Item $Copyright (Join-Path $Licenses ($_.Name + '-LICENSE')) }
}
$Source = Join-Path $Stage 'Source'
New-Item -ItemType Directory -Force -Path (Join-Path $Source 'Wheeler-Refined') | Out-Null
foreach ($Name in @('src','cmake','CMakeLists.txt','CMakePresets.json','vcpkg.json','vcpkg-configuration.json','.clang-format','.editorconfig','LICENSE','LICENSES','NOTICE.md','THIRD_PARTY_NOTICES.md','BUILDING.md','README.md','tools')) {
    Copy-Item (Join-Path $Refined $Name) (Join-Path $Source 'Wheeler-Refined') -Recurse
}
foreach ($Name in @('tools','tests','wheeler-compat.patch','README.md')) {
    Copy-Item (Join-Path $Root $Name) $Source -Recurse
}
$Info = [ordered]@{
    patchVersion = '1.1.0'; builtUTC = [DateTime]::UtcNow.ToString('o')
    refinedCommit = $RefinedCommit; commonLibCommit = $CommonCommit; vcpkgCommit = $VcpkgCommit
    patchCommit = $env:GITHUB_SHA
    targetRuntime = 'Steam Skyrim SE 1.6.1170'; targetRefined = '1.3.3.0'
    dllSHA256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLower()
    patchSHA256 = (Get-FileHash (Join-Path $Root 'wheeler-compat.patch') -Algorithm SHA256).Hash.ToLower()
    automatedTests = '16 potion batch selection, depletion, legacy migration and UTF-8 checks; helper API tests run in companion build'
    features = 'transferred enchantment descriptions; existing potion rename compatibility'
    requiresDescriptionHelper = 'EnchantmentSwapperDescriptions 1.1.0 for transferred descriptions'
    inGameTested = $false
}
$Info | ConvertTo-Json | Set-Content (Join-Path $Stage 'BUILD-INFO.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Wheeler_Refined_Enchantment_Descriptions_v1_1.zip') -CompressionLevel Optimal
