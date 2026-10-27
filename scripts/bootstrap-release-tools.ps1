param([switch]$SkipInstallerTools)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$versions=Get-Content -LiteralPath "$PSScriptRoot\tool-versions.json" -Raw | ConvertFrom-Json
$downloadRoot=Join-Path $projectRoot 'tools\downloads'
New-Item -ItemType Directory -Path $downloadRoot -Force | Out-Null
function Get-VerifiedTool($metadata,[string]$target){
    if(-not(Test-Path -LiteralPath $target)){Invoke-WebRequest -Uri $metadata.url -OutFile $target}
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $metadata.sha256){throw "Tool hash mismatch: $target"}
    $signature=Get-AuthenticodeSignature -LiteralPath $target
    if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch [regex]::Escape($metadata.signer)){throw "Tool signature invalid: $target"}
}
if(-not $SkipInstallerTools){
    $compilerSetup=Join-Path $downloadRoot 'innosetup-7.1.0-x64.exe'
    Get-VerifiedTool $versions.innoSetup $compilerSetup
    Get-VerifiedTool $versions.runtime (Join-Path $downloadRoot 'vc_redist.x64.exe')
    $compilerRoot=Join-Path $projectRoot 'tools\innosetup'
    if(-not(Test-Path -LiteralPath "$compilerRoot\ISCC.exe")){
        $setup=Start-Process -FilePath $compilerSetup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/CURRENTUSER','/NOICONS',('/DIR="'+$compilerRoot+'"'),'/TASKS=""') -WindowStyle Hidden -PassThru -Wait
        if($setup.ExitCode -ne 0){throw "Inno Setup compiler installation failed: $($setup.ExitCode)"}
    }
}
$sdkPath=Join-Path $projectRoot 'tools\vst3sdk'
if(-not(Test-Path -LiteralPath "$sdkPath\.git")){
    & git clone --depth 1 --branch $versions.steinberg.tag $versions.steinberg.repository $sdkPath
    if($LASTEXITCODE -ne 0){throw 'Steinberg SDK clone failed.'}
}
$revision=& git -C $sdkPath rev-parse HEAD
if($LASTEXITCODE -ne 0 -or $revision -ne $versions.steinberg.commit){throw 'Steinberg SDK revision differs from pinned tool-versions.json.'}
& git -C $sdkPath submodule update --init --depth 1 base pluginterfaces public.sdk cmake
if($LASTEXITCODE -ne 0){throw 'Steinberg submodule download failed.'}
& "$PSScriptRoot\build.ps1" -ConfigureOnly
$vswherePath=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswherePath -latest -products '*' -property installationPath
$cmakePath=Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninjaPath=Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
& $cmakePath -S $sdkPath -B "$projectRoot\build\steinberg-validator" -G 'Ninja Multi-Config' "-DCMAKE_MAKE_PROGRAM=$ninjaPath" -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_CREATE_PLUGIN_LINK=OFF
if($LASTEXITCODE -ne 0){throw 'Steinberg validator configure failed.'}
& $cmakePath --build "$projectRoot\build\steinberg-validator" --config Release --target validator --parallel 4
if($LASTEXITCODE -ne 0){throw 'Steinberg validator build failed.'}
Write-Output 'Release tools ready. pluginval is a separate official download: tools/pluginval/pluginval.exe.'
