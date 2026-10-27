param([switch]$UnsignedPreview,[ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$Thumbprint='',
      [ValidateSet('CurrentUser','LocalMachine')][string]$CertificateStore='CurrentUser',
      [string]$TimestampUrl='http://timestamp.digicert.com')
$ErrorActionPreference='Stop'
if(-not $UnsignedPreview -and -not $Thumbprint){throw 'A trusted signing thumbprint is required. Use -UnsignedPreview only for an explicitly unsigned preview.'}
if($UnsignedPreview -and $Thumbprint){throw 'Choose a signed release or an unsigned preview, not both.'}
$projectRoot=Split-Path -Parent $PSScriptRoot
$version=[regex]::Match((Get-Content -LiteralPath "$projectRoot\CMakeLists.txt" -Raw),'project\(Emberfox\s+VERSION\s+([0-9.]+)').Groups[1].Value
if(-not $version){throw 'Project version missing.'}
$compiler=Join-Path $projectRoot 'tools\innosetup\ISCC.exe'
$runtime=Join-Path $projectRoot 'tools\downloads\vc_redist.x64.exe'
if(-not(Test-Path -LiteralPath $compiler) -or -not(Test-Path -LiteralPath $runtime)){throw 'Run scripts/bootstrap-release-tools.ps1 before building the installer.'}
$versions=Get-Content -LiteralPath "$PSScriptRoot\tool-versions.json" -Raw | ConvertFrom-Json
if((Get-FileHash -LiteralPath $runtime -Algorithm SHA256).Hash -ne $versions.runtime.sha256){throw 'Bundled runtime differs from pinned Microsoft download.'}
$runtimeSignature=Get-AuthenticodeSignature -LiteralPath $runtime
if($runtimeSignature.Status -ne 'Valid' -or $runtimeSignature.SignerCertificate.Subject -notmatch 'Microsoft Corporation'){throw 'Bundled runtime signature invalid.'}
if(-not $UnsignedPreview){
    & "$PSScriptRoot\sign-release.ps1" -Path @("$projectRoot\build\msvc\Emberfox_artefacts\Release\Standalone\EMBERFOX.exe","$projectRoot\build\msvc\Emberfox_artefacts\Release\VST3\EMBERFOX.vst3\Contents\x86_64-win\EMBERFOX.vst3") -Thumbprint $Thumbprint -Store $CertificateStore -TimestampUrl $TimestampUrl
    & "$PSScriptRoot\validate.ps1"
}
& "$PSScriptRoot\package.ps1"
$stage=Join-Path $projectRoot ('build\installer-'+[guid]::NewGuid().ToString('N'))
$payload=Join-Path $stage 'payload'
New-Item -ItemType Directory -Path $payload -Force | Out-Null
$plugin=Join-Path $projectRoot 'build\msvc\Emberfox_artefacts\Release\VST3\EMBERFOX.vst3'
Copy-Item -LiteralPath $plugin -Destination $payload -Recurse
Copy-Item -LiteralPath "$projectRoot\build\msvc\Emberfox_artefacts\Release\Standalone\EMBERFOX.exe" -Destination $payload
Copy-Item -LiteralPath "$projectRoot\docs\customer" -Destination "$payload\Documentation" -Recurse
foreach($file in @('LICENSE.txt','THIRD_PARTY.md')){Copy-Item -LiteralPath "$projectRoot\$file" -Destination $payload}
Copy-Item -LiteralPath "$projectRoot\dist\EMBERFOX-$version-source.zip" -Destination $payload
$status=if($UnsignedPreview){'unsigned-preview'}else{'signed'}
@{version=$version;publisher='EmberSoft';status=$status;license='AGPL-3.0-only';supportContact=$null} | ConvertTo-Json | Set-Content -LiteralPath "$payload\release-status.json"
$installerName="EMBERFOX-$version-setup"
if($UnsignedPreview){$installerName+='-unsigned'}
$arguments=@("/DAppVersion=$version","/DPayloadDir=$payload","/DRuntimeFile=$runtime","/DOutputDir=$projectRoot\dist","/DInstallerFileName=$installerName")
if(-not $UnsignedPreview){
    $powerShellPath=(Get-Process -Id $PID).Path
    # Inno expands $q after parsing /S; literal embedded quotes break paths containing spaces.
    # $f is already a quoted filename supplied by Inno.
    $signCommand='$q'+$powerShellPath+'$q -NoProfile -File $q'+$PSScriptRoot+'\sign-release.ps1$q -Thumbprint '+$Thumbprint+' -Store '+$CertificateStore+' -TimestampUrl $q'+$TimestampUrl+'$q -Path $f'
    $arguments+='/DSignedBuild=1';$arguments+=('/SEmberSoft='+$signCommand)
}
& $compiler @arguments "$projectRoot\installer\EMBERFOX.iss"
if($LASTEXITCODE -ne 0){throw 'Inno Setup compilation failed; staging preserved for diagnostics.'}
$installerPath=Join-Path $projectRoot "dist\$installerName.exe"
if(-not $UnsignedPreview){
    $signature=Get-AuthenticodeSignature -LiteralPath $installerPath
    if($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate){throw 'Installer is not validly timestamp-signed.'}
}
$manifest=[ordered]@{version=$version;publisher='EmberSoft';status=$status;installer=(Split-Path -Leaf $installerPath);installerSha256=(Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash;
    pluginSha256=(Get-FileHash -LiteralPath "$payload\EMBERFOX.vst3\Contents\x86_64-win\EMBERFOX.vst3" -Algorithm SHA256).Hash;
    standaloneSha256=(Get-FileHash -LiteralPath "$payload\EMBERFOX.exe" -Algorithm SHA256).Hash;
    sourceSha256=(Get-FileHash -LiteralPath "$projectRoot\dist\EMBERFOX-$version-source.zip" -Algorithm SHA256).Hash}
$manifest | ConvertTo-Json | Set-Content -LiteralPath "$projectRoot\dist\EMBERFOX-$version-installer-manifest.json"
$absoluteStage=[IO.Path]::GetFullPath($stage)
$absoluteBuild=[IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))+[IO.Path]::DirectorySeparatorChar
if(-not $absoluteStage.StartsWith($absoluteBuild,[StringComparison]::OrdinalIgnoreCase)){throw 'Installer stage escaped project build directory.'}
Remove-Item -LiteralPath $absoluteStage -Recurse -Force
Write-Output "Installer created: $installerPath ($status)"
