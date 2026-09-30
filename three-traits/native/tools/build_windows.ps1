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
$CompilerArchive = Join-Path $WorkDirectory 'papyrus-compiler-windows.zip'
Invoke-WebRequest 'https://github.com/russo-2025/papyrus-compiler/releases/download/2026.03.15/papyrus-compiler-windows.zip' -OutFile $CompilerArchive
$CompilerHash = (Get-FileHash $CompilerArchive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($CompilerHash -ne '67b44c77d00a5cda986bec6af5c228a56abe6ec1fadcb4cfea5c858e6941140e') { throw 'Papyrus compiler archive hash mismatch' }
$CompilerRoot = Join-Path $WorkDirectory 'papyrus-compiler'
Expand-Archive $CompilerArchive $CompilerRoot
$CompilerCandidates = @(Get-ChildItem $CompilerRoot -Recurse -File | Where-Object { $_.Name -in @('papyrus.exe', 'papyrus-compiler.exe') })
if ($CompilerCandidates.Count -ne 1) { throw 'Expected exactly one Papyrus compiler executable' }
$Compiler = $CompilerCandidates[0].FullName
$PapyrusOutput = Join-Path $WorkDirectory 'compiled-papyrus'
New-Item -ItemType Directory -Force -Path $PapyrusOutput | Out-Null
Run $Compiler @('compile', '-nocache', '-h', (Join-Path $Root 'papyrus/headers'), '-i', (Join-Path $Root 'papyrus/GT_GreybeardTrainedEffect.psc'), '-o', $PapyrusOutput)
$Pex = Join-Path $PapyrusOutput 'GT_GreybeardTrainedEffect.pex'
if (!(Test-Path $Pex)) { throw 'Retired Voice script was not compiled' }
$PexDump = (& $Compiler read $Pex | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'Papyrus inspection failed' }
if ($PexDump -match '(?i)ident\((AddShout|TeachWord|UnlockWord|Show|RegisterForSingleUpdate)\)') { throw 'Retired Voice script still contains a grant or update-registration call' }
if ($PexDump -notmatch '(?i)ident\(UnregisterForUpdate\)') { throw 'Retired Voice script is missing update cleanup' }
$PexDump | Set-Content (Join-Path $PapyrusOutput 'PEX-Inspection.txt') -Encoding utf8
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
Run $MSBuild @((Join-Path $Build 'BiggieTraitMechanics.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
# Copy only this DLL: CommonLib's install rules are not part of the mod package.
$PluginDirectory = Join-Path $Stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force -Path $PluginDirectory | Out-Null
$Dll = Join-Path $Build 'Release/BiggieTraitMechanics.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
Copy-Item $Dll $PluginDirectory
Copy-Item (Join-Path $Root 'BiggieTraitMechanics.ini') $PluginDirectory
Copy-Item (Join-Path $Root 'README.md') $Stage
Copy-Item (Join-Path $Root 'LICENSE') $Stage
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Stage 'CommonLibSSE-LICENSE')
New-Item -ItemType Directory -Force -Path (Join-Path $Stage 'Scripts'), (Join-Path $Stage 'Source/Scripts') | Out-Null
Copy-Item $Pex (Join-Path $Stage 'Scripts/GT_GreybeardTrainedEffect.pex')
Copy-Item (Join-Path $Root 'papyrus/GT_GreybeardTrainedEffect.psc') (Join-Path $Stage 'Source/Scripts/GT_GreybeardTrainedEffect.psc')
Copy-Item (Join-Path $PapyrusOutput 'PEX-Inspection.txt') $Stage
$Sources = [ordered]@{}
$SourceNames = @('CMakeLists.txt','vcpkg.json','BiggieTraitMechanics.ini','README.md','tools/build_windows.ps1')
foreach ($Folder in @('src','tests','papyrus')) {
    Get-ChildItem (Join-Path $Root $Folder) -Recurse -File | Sort-Object FullName | ForEach-Object {
        $SourceNames += [IO.Path]::GetRelativePath($Root, $_.FullName).Replace('\','/')
    }
}
foreach ($Name in $SourceNames) {
    # Normalize checkout CRLF for reproducible source identification.
    $Text = [IO.File]::ReadAllText((Join-Path $Root $Name)).Replace("`r`n", "`n")
    $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $Sources[$Name] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
$Info = [ordered]@{
    plugin = 'BiggieTraitMechanics'; version = '1.7.0'; runtime = '1.6.1170'
    source_commit = $env:GITHUB_SHA; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources; windows_build = 'passed'; rules_tests = 'passed'; native_test_suites = 11; dynamo_charge_tests = 'passed'; scoped_charge_cost = $true; original_cost_for_magicka_payment = $true; direct_and_virtual_cast_paths = $true; fixed_resource_call_offsets_removed = $true; in_game_tested = $false
    voice_authority_upgrade_tests = 'passed'; legacy_shout_script_inert = $true
    papyrus_compiler = 'russo-2025/papyrus-compiler 2026.03.15'; papyrus_compiler_archive_sha256 = $CompilerHash
    retired_voice_pex_sha256 = (Get-FileHash $Pex -Algorithm SHA256).Hash.ToLowerInvariant()
    combined_version = "2.11.0-beta1"; no_new_inventory_items = $true
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Biggie_Trait_Mechanics_v1.zip')
