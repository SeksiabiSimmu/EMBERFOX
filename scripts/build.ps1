param([string]$Configuration = 'Release', [switch]$ConfigureOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path -LiteralPath "$projectRoot\external\JUCE\CMakeLists.txt") -and
    (Test-Path -LiteralPath "$projectRoot\external\JUCE-source.zip")) {
    Expand-Archive -LiteralPath "$projectRoot\external\JUCE-source.zip" -DestinationPath "$projectRoot\external" -Force
}
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswherePath -latest -products '*' -property installationPath
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) { $cmakePath = $cmakeCommand.Source }
else {
    $cmakePath = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not (Test-Path -LiteralPath $cmakePath)) { throw 'Install Visual Studio C++ Build Tools with CMake and a Windows SDK.' }
$devCommand = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$developerEnvironment = & $env:ComSpec /d /c "`"$devCommand`" -arch=x64 -host_arch=x64 -vcvars_ver=14.50 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'MSVC developer environment initialization failed.' }
foreach ($entry in $developerEnvironment) {
    if ($entry -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
$ninjaPath = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
& $cmakePath -S $projectRoot -B "$projectRoot\build\msvc" -G 'Ninja Multi-Config' "-DCMAKE_MAKE_PROGRAM=$ninjaPath"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
if (-not $ConfigureOnly) {
    & $cmakePath --build "$projectRoot\build\msvc" --config $Configuration --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
}
