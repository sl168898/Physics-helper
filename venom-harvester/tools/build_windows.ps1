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
Run $MSBuild @((Join-Path $Build 'VenomHarvester.vcxproj'), '/t:ClCompile', '/p:Configuration=Release', '/p:Platform=x64', '/p:BuildProjectReferences=false', '/verbosity:minimal')
Run 'cmake' @('--build', $Build, '--config', 'Release', '--parallel', '2')
Run 'ctest' @('--test-dir', $Build, '-C', 'Release', '--output-on-failure')
# Copy only this DLL: CommonLib's install rules are not part of the mod package.
$PluginDirectory = Join-Path $Stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force -Path $PluginDirectory | Out-Null
$Dll = Join-Path $Build 'Release/VenomHarvester.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced' }
Copy-Item $Dll $PluginDirectory
Copy-Item (Join-Path $Root 'README.md') $Stage
Copy-Item (Join-Path $Root 'LICENSE') $Stage
Copy-Item (Join-Path $Common 'LICENSE') (Join-Path $Stage 'CommonLibSSE-LICENSE')
$Sources = [ordered]@{}
foreach ($Name in @('src/main.cpp','src/Harvest.h','src/MenuGate.h','src/CraftCapture.h','src/DeferredForms.h','src/PendingCrafts.h','tests/harvest_tests.cpp','tests/menu_tests.cpp','tests/crafting_tests.cpp','tests/inventory_event_tests.cpp','tests/pending_crafts_tests.cpp','CMakeLists.txt','vcpkg.json')) {
    # Normalize checkout CRLF for reproducible source identification.
    $Text = [IO.File]::ReadAllText((Join-Path $Root $Name)).Replace("`r`n", "`n")
    $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $Sources[$Name] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
$Info = [ordered]@{
    plugin = 'VenomHarvester'; version = '2.0.7'; runtime = '1.6.1170'
    source_commit = $env:GITHUB_SHA; commonlib_commit = $CommonCommit; vcpkg_commit = $VcpkgCommit
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256_lf = $Sources; windows_build = 'passed'; harvest_tests = 'passed'; menu_tests = 'passed'; crafting_tests = 'passed'; inventory_event_tests = 'passed'; pending_crafts_tests = 'passed'; in_game_tested = $false
    trait = "Huntsman's Satchel"; combined_package_required = '2.8.0-beta1'
    poison_resistance_penalty = 50; outgoing_poison_multiplier = 1.0
    unique_native_poison_per_batch = $true; no_new_esp = $true
    confirmed_kill_refund_requires_corpse = $false; pending_kill_survives_corpse_deletion = $true
    crafted_poison_identity = 'unique positive unbound poison inventory delta within captured alchemy callback'
    generic_craft_event_is_output_identity = $false
    release_type = 'beta'; native_poison_creation_api_corrected = $true
    native_creation_api = 'BGSCreatedObjectManager::AddPoison'; native_creation_ids = @(35266, 36168)
    poison_classification_required_before_metadata_copy = $true
    native_copy_rejection_reasons_logged = $true; original_and_created_effects_logged = $true
    inventory_event_form_retention = 'native references acquired before exchange and released after previously queued SKSE tasks'
    retention_cleanup_uses_form_ids_and_save_generation = $true
    paper_dll_modified = $false; arbitrary_timer_delay = $false; permanent_reference_pins = $false
    creation_and_exchange_wait_for_crafting_menu_destruction = $true
    exchange_waits_for_furniture_release = $true
    expenditure_captured_synchronously = $true; source_retained_until_menu_close = $true
    pending_crafts_cosave_record = 'HSAP v1'; bound_batches_cosave_record = 'HSAT v2 unchanged'
    crafting_inventory_extender_dll_modified = $false
}
$Info | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath (Join-Path $WorkDirectory 'Huntsmans_Satchel_SKSE_v2_0_7_beta1.zip')
