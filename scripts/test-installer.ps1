param([string]$InstallerPath='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$version=[regex]::Match((Get-Content -LiteralPath "$projectRoot\CMakeLists.txt" -Raw),'project\(Emberfox\s+VERSION\s+([0-9.]+)').Groups[1].Value
if(-not $InstallerPath){$InstallerPath=Join-Path $projectRoot "dist\EMBERFOX-$version-setup-unsigned.exe"}
$InstallerPath=(Resolve-Path -LiteralPath $InstallerPath).Path
$reportRoot=Join-Path $projectRoot 'docs\validation'
$testRoot=Join-Path $projectRoot ('build\installer-smoke-'+[guid]::NewGuid().ToString('N'))
$absoluteBuild=[IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))+[IO.Path]::DirectorySeparatorChar
if(-not [IO.Path]::GetFullPath($testRoot).StartsWith($absoluteBuild,[StringComparison]::OrdinalIgnoreCase)){throw 'Installer test root escaped build.'}
$appPath=Join-Path $testRoot 'Application'
$pluginRoot=Join-Path $testRoot 'VST3'
$pluginPath=Join-Path $pluginRoot 'EMBERFOX.vst3'
$uninstallKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{49A55B6A-451C-4559-9572-68E7B9D381B1}_is1'
$productKey='HKCU:\Software\EmberSoft\EMBERFOX'
if((Test-Path -LiteralPath $uninstallKey) -or (Test-Path -LiteralPath $productKey)){throw 'An existing per-user installer registration exists. Use a clean test account/VM; this test will not overwrite it.'}
$runtimeBefore=Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64'
if(-not($runtimeBefore.Installed -eq 1 -and ($runtimeBefore.Minor -gt 50 -or ($runtimeBefore.Minor -eq 50 -and $runtimeBefore.Bld -ge 35717)))){throw 'This workspace smoke requires an existing suitable runtime; prerequisite installation belongs in the clean-VM test.'}
$runtimeHash=(Get-FileHash -LiteralPath "$env:WINDIR\System32\vcruntime140.dll" -Algorithm SHA256).Hash
New-Item -ItemType Directory -Path $appPath,$pluginRoot -Force | Out-Null
$notes=Join-Path $appPath 'user-notes.txt'
$sibling=Join-Path $pluginRoot 'unrelated-plugin.txt'
Set-Content -LiteralPath $notes 'User-owned file: preserve through upgrade/uninstall.'
Set-Content -LiteralPath $sibling 'Unrelated VST3 folder contents: preserve.'
$checks=[Collections.Generic.List[string]]::new()
$checks.Add("EMBERFOX $version actual installer workspace smoke. This is not a clean Windows test.")
Add-Type -AssemblyName System.IO.Compression.FileSystem
$baseline=Join-Path $projectRoot 'dist\EMBERFOX-1.2.1-windows-x64.zip'
if(Test-Path -LiteralPath $baseline){
    $archive=[IO.Compression.ZipFile]::OpenRead($baseline)
    try{
        foreach($entry in $archive.Entries){
            $name=$entry.FullName.Replace('\','/')
            if(($name.StartsWith('EMBERFOX.vst3/') -and -not $name.EndsWith('/')) -or $name -eq 'EMBERFOX.exe'){
                $target=if($name -eq 'EMBERFOX.exe'){Join-Path $appPath $name}else{Join-Path $pluginRoot $name}
                $absolute=[IO.Path]::GetFullPath($target)
                if(-not $absolute.StartsWith([IO.Path]::GetFullPath($testRoot)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Baseline archive path escaped test root.'}
                New-Item -ItemType Directory -Path (Split-Path -Parent $absolute) -Force | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$absolute,$true)
            }
        }
    }finally{$archive.Dispose()}
    $checks.Add('Prepared a real 1.2.1 manual-install baseline in isolated destinations.')
}
for($pass=1;$pass -le 2;$pass++){
    $logPath=Join-Path $reportRoot "installer-pass$pass.log"
    $setup=Start-Process -FilePath $InstallerPath -ArgumentList @('/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/NOICONS',('/DIR="'+$appPath+'"'),('/VST3DIR="'+$pluginRoot+'"'),('/LOG="'+$logPath+'"')) -WindowStyle Hidden -PassThru -Wait
    if($setup.ExitCode -ne 0){throw "Installer pass$pass failed: $($setup.ExitCode)"}
    foreach($file in @("$appPath\EMBERFOX.exe","$pluginPath\Contents\x86_64-win\EMBERFOX.vst3","$appPath\Documentation\UserGuide.html","$appPath\Source\EMBERFOX-$version-source.zip")){if(-not(Test-Path -LiteralPath $file)){throw "Installed file missing: $file"}}
    $installedHash=(Get-FileHash -LiteralPath "$pluginPath\Contents\x86_64-win\EMBERFOX.vst3" -Algorithm SHA256).Hash
    $manifest=Get-Content -LiteralPath "$projectRoot\dist\EMBERFOX-$version-installer-manifest.json" -Raw | ConvertFrom-Json
    if($installedHash -ne $manifest.pluginSha256){throw 'Installed VST3 differs from installer manifest.'}
    if((Get-FileHash -LiteralPath "$appPath\EMBERFOX.exe" -Algorithm SHA256).Hash -ne $manifest.standaloneSha256){throw 'Installed standalone differs.'}
    if((Get-FileHash -LiteralPath "$appPath\Source\EMBERFOX-$version-source.zip" -Algorithm SHA256).Hash -ne $manifest.sourceSha256){throw 'Installed corresponding source differs.'}
    if(-not(Test-Path -LiteralPath $notes) -or -not(Test-Path -LiteralPath $sibling)){throw 'Installer removed user-created files.'}
    if((Get-Content -LiteralPath $logPath -Raw) -notmatch 'no shared runtime changes'){throw 'Runtime version guard did not use the existing compatible runtime.'}
    $checks.Add("PASS install pass${pass}: upgrade/reinstall, complete bundle/docs/source hashes, runtime skip and user-file preservation.")
}
$app=Start-Process -FilePath "$appPath\EMBERFOX.exe" -WindowStyle Hidden -PassThru
try{if(-not $app.WaitForInputIdle(5000)){throw 'Installed standalone did not become ready.'};$app.Refresh();if($app.HasExited){throw 'Installed standalone exited.'}}finally{$app.Refresh();if(-not $app.HasExited){Stop-Process -Id $app.Id};$app.Dispose()}
$checks.Add('PASS installed standalone startup.')
$hostPath=Join-Path $projectRoot 'build\msvc\EmberfoxHostSmoke_artefacts\Release\EmberfoxHostSmoke.exe'
& $hostPath $pluginPath (Join-Path $testRoot 'host-output') *> "$reportRoot\installed-host-results.txt"
if($LASTEXITCODE -ne 0){throw 'Actual installed VST3 host smoke failed.'}
$checks.Add('PASS actual installed VST3 host audio/automation/state/lifecycle suite.')
$uninstall=Start-Process -FilePath "$appPath\unins000.exe" -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+$reportRoot+'\installer-uninstall.log"')) -WindowStyle Hidden -PassThru -Wait
if($uninstall.ExitCode -ne 0){throw "Uninstall failed: $($uninstall.ExitCode)"}
if((Test-Path -LiteralPath "$appPath\EMBERFOX.exe") -or (Test-Path -LiteralPath "$pluginPath\Contents\x86_64-win\EMBERFOX.vst3")){throw 'Uninstaller left owned executables.'}
if(-not(Test-Path -LiteralPath $notes) -or -not(Test-Path -LiteralPath $sibling)){throw 'Uninstaller removed user-owned files.'}
if((Test-Path -LiteralPath $uninstallKey) -or (Test-Path -LiteralPath $productKey)){throw 'Uninstaller left its registration.'}
if((Get-FileHash -LiteralPath "$env:WINDIR\System32\vcruntime140.dll" -Algorithm SHA256).Hash -ne $runtimeHash){throw 'Shared Microsoft runtime changed.'}
$checks.Add('PASS uninstall: owned binaries/registration removed, user files/unrelated plugins and shared runtime preserved.')
$checks.Add("Retained test folder for inspection: $testRoot")
$checks | Set-Content -LiteralPath "$reportRoot\installer-results.txt"
$checks
