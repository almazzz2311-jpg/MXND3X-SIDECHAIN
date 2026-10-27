param([string]$JuceDir = "", [int]$Jobs = 4)
$ErrorActionPreference = "Stop"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw "Install CMake 3.22+ and add it to PATH." }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Install Visual Studio 2022 with Desktop development with C++." }
$vs = & $vswhere -latest -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio 2022 x64 C++ tools not found." }
$root = Split-Path $PSScriptRoot -Parent
$argsList = @('-S', $root, '-B', "$root\build-windows", '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DBUILD_TESTING=ON')
if ($JuceDir) {
 if (-not (Test-Path "$JuceDir\CMakeLists.txt")) { throw "Invalid JUCE source directory: $JuceDir" }
 $argsList += "-DJUCE_DIR=$JuceDir"
} else { Write-Host 'JUCE 8.0.6 will be fetched from its pinned Git commit; Git and network access are required.' }
& cmake @argsList
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build "$root\build-windows" --config Release --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
& ctest --test-dir "$root\build-windows" -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
$plugin = "$root\build-windows\MXND3XSidechain_artefacts\Release\VST3\MXND3X SIDECHAIN.vst3"
if (-not (Test-Path "$plugin\Contents\x86_64-win\MXND3X SIDECHAIN.vst3")) { throw "Windows x64 VST3 binary missing: $plugin" }
New-Item -ItemType Directory -Force "$root\dist" | Out-Null
Copy-Item -Recurse -Force $plugin "$root\dist"
Write-Host "Built: $root\dist\MXND3X SIDECHAIN.vst3"
Write-Host 'Copy the whole .vst3 bundle to C:\Program Files\Common Files\VST3\ (administrator permission may be needed).'
