# Experimental build recipe; requires Windows + VS 2022 Desktop C++ + CMake + Git.
# This script has NOT been executed in the Linux authoring environment.
[CmdletBinding()]
param([string]$WorkDirectory = (Join-Path $PSScriptRoot '..\_build'))

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'Use Windows with Visual Studio 2022 C++ build tools.' }
$Bundle = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
$FsmpCommit = '1bdafeff57cc7a93fd940ef899490ce60053b750'
# Baseline from the pinned FSMP v3.2.1 root vcpkg.json manifest.
$VcpkgCommit = '60b06921c7c7ac787b23a222dfab5cdd3911712e'
$Fsmp = Join-Path $WorkDirectory 'fsmp'
$Vcpkg = Join-Path $WorkDirectory 'vcpkg'
$Patch = Join-Path $Bundle 'native.patch'

function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

function ClonePinned([string]$Url, [string]$Destination, [string]$Commit) {
    if (Test-Path $Destination) {
        throw "Build directory already exists: $Destination. Use a new -WorkDirectory to avoid overwriting work."
    }
    Run 'git' @('clone', '--no-checkout', $Url, $Destination)
    Run 'git' @('-C', $Destination, 'checkout', '--detach', $Commit)
    $Actual = (& git -C $Destination rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $Actual -ne $Commit) { throw 'Dependency revision mismatch.' }
}

Get-Command git, cmake -ErrorAction Stop | Out-Null
New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null

# Only source code is uploaded to this public repository.
# The mod-derived companion ESP is assembled locally after the DLL build.

ClonePinned 'https://github.com/DaymareOn/hdtSMP64.git' $Fsmp $FsmpCommit
Run 'git' @('-C', $Fsmp, 'apply', '--check', $Patch)
Run 'git' @('-C', $Fsmp, 'apply', $Patch)
Run 'git' @('-C', $Fsmp, 'submodule', 'update', '--init', '--recursive')
ClonePinned 'https://github.com/microsoft/vcpkg.git' $Vcpkg $VcpkgCommit
Run (Join-Path $Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')

# These variables apply only to this PowerShell process and its children.
$env:VCPKG_ROOT = $Vcpkg
$env:CommonLibSSEPath = Join-Path $Fsmp 'extern\CommonLibSSE'
$env:CompiledPluginsPath = Join-Path $WorkDirectory 'unused-copy-output'
Push-Location $Fsmp
try {
    Run 'cmake' @('--preset', 'vs2022-windows', '-DCOPY_OUTPUT=OFF',
        '-DENABLE_SKYRIM_SE=ON', '-DENABLE_SKYRIM_AE=ON', '-DENABLE_SKYRIM_VR=OFF',
        '-DPROJECT_BUILD_INFO=flail-p2-fsmp321')
    Run 'cmake' @('--build', 'out/build/vs2022-windows', '--config', 'Release', '--parallel', '2')
    $Stage = Join-Path $WorkDirectory 'mod-stage'
    Run 'cmake' @('--install', 'out/build/vs2022-windows', '--config', 'Release', '--component', 'main', '--prefix', $Stage)
} finally {
    Pop-Location
}

$Dll = Join-Path $Stage 'SKSE\Plugins\hdtsmp64.dll'
if (!(Test-Path $Dll) -or (Get-Item $Dll).Length -eq 0) { throw 'No DLL produced; refusing to package a mod.' }
Copy-Item (Join-Path $Bundle 'runtime\SKSE\Plugins\FlailDirectSMP.ini') (Join-Path $Stage 'SKSE\Plugins')
Copy-Item (Join-Path $Bundle 'RUNTIME_TESTING.md') (Join-Path $Stage 'README.md')
Copy-Item (Join-Path $Bundle 'LICENSE') $Stage
Copy-Item (Join-Path $Bundle 'EXCEPTIONS') $Stage
$Info = [ordered]@{
    prototype = 'p2-silver-only-fsmp321'
    fsmp_source_commit = $FsmpCommit
    vcpkg_commit = $VcpkgCommit
    patch_sha256 = (Get-FileHash $Patch -Algorithm SHA256).Hash.ToLowerInvariant()
    dll_sha256 = (Get-FileHash $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    native_dll_built = $true
    contains_companion_esp = $false
    in_game_tested = $false
    cpu_target = 'baseline x64, no CUDA'
    runtime_target = 'User target: Skyrim 1.6.1170 with FSMP 3.2.1; runtime compatibility not tested'
}
$Info | ConvertTo-Json | Set-Content (Join-Path $Stage 'BuildInfo.json') -Encoding utf8
$Zip = Join-Path $WorkDirectory 'FlailDirectSMP_native_p2_UNTESTED.zip'
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $Zip
Write-Host "Native-only Windows build complete. Companion ESP is not included: $Zip"
