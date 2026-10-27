$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& "$PSScriptRoot\test-release-metadata.ps1"
$versionMatch = [regex]::Match((Get-Content -LiteralPath "$projectRoot\CMakeLists.txt" -Raw), 'project\(Emberfox\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) { throw 'Cannot determine the project version.' }
$packageBase = 'EMBERFOX-' + $versionMatch.Groups[1].Value
$distPath = Join-Path $projectRoot 'dist'
$stagePath = Join-Path $projectRoot ('build\package-' + [guid]::NewGuid().ToString('N'))
$sourceStage = Join-Path $stagePath 'source'
$binaryStage = Join-Path $stagePath 'binary'
New-Item -ItemType Directory -Path $distPath,$sourceStage,$binaryStage -Force | Out-Null
foreach ($entry in @('Source','Tests','scripts','installer','docs','CMakeLists.txt','README.md','LICENSE.txt','THIRD_PARTY.md','.gitignore','.gitattributes')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $entry) -Destination $sourceStage -Recurse -Force
}
New-Item -ItemType Directory -Path "$sourceStage\external" -Force | Out-Null
$vendorSource = Join-Path $projectRoot 'external\JUCE'
if (-not (Test-Path -LiteralPath "$vendorSource\.git")) {
    $fetchedSource = Join-Path $projectRoot 'build\msvc\_deps\juce-src'
    if (Test-Path -LiteralPath "$fetchedSource\.git") { $vendorSource = $fetchedSource }
}
$vendorArchive = Join-Path $sourceStage 'external\JUCE-source.zip'
if (Test-Path -LiteralPath "$vendorSource\.git") {
    & git -C $vendorSource archive --format=zip --prefix=JUCE/ "--output=$vendorArchive" ea77444750de45e036e0279bb0291144bf8e3688
    if ($LASTEXITCODE -ne 0) { throw 'Pinned JUCE source archive failed.' }
} elseif (Test-Path -LiteralPath "$projectRoot\external\JUCE-source.zip") {
    Copy-Item -LiteralPath "$projectRoot\external\JUCE-source.zip" -Destination $vendorArchive
} else { throw 'Missing pinned JUCE source: cannot create complete corresponding source package.' }
# Publish useful evidence without exposing machine-specific home/workspace paths.
function Protect-PackageDocumentation([string]$root) {
    foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse) {
        if ($file.Extension -notin @('.md','.txt','.log','.html')) { continue }
        $original = Get-Content -LiteralPath $file.FullName -Raw
        if (-not $original) { continue }
        $public = $original
        foreach ($pair in @(@($projectRoot,'<project>'),@($env:USERPROFILE,'<user>'))) {
            if (-not $pair[0]) { continue }
            foreach ($variant in @($pair[0],$pair[0].Replace('\','/'),$pair[0].Replace('\','\\'))) {
                $public = [regex]::Replace($public,[regex]::Escape($variant),$pair[1],[Text.RegularExpressions.RegexOptions]::IgnoreCase)
            }
        }
        if ($public -ne $original) { Set-Content -LiteralPath $file.FullName -Value $public -NoNewline }
    }
}
Protect-PackageDocumentation (Join-Path $sourceStage 'docs')
Compress-Archive -Path "$sourceStage\*" -DestinationPath "$distPath\$packageBase-source.zip" -Force
$pluginSource = Join-Path $projectRoot 'build\msvc\Emberfox_artefacts\Release\VST3\EMBERFOX.vst3'
if (-not (Test-Path -LiteralPath "$pluginSource\Contents\x86_64-win\EMBERFOX.vst3")) { throw 'Build the Release VST3 before packaging.' }
Copy-Item -LiteralPath $pluginSource -Destination $binaryStage -Recurse -Force
Copy-Item -LiteralPath "$projectRoot\build\msvc\Emberfox_artefacts\Release\Standalone\EMBERFOX.exe" -Destination $binaryStage
foreach ($entry in @('LICENSE.txt','THIRD_PARTY.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $entry) -Destination $binaryStage
}
Copy-Item -LiteralPath "$projectRoot\docs\customer\README.md" -Destination "$binaryStage\README.md"
Copy-Item -LiteralPath "$projectRoot\docs\customer" -Destination "$binaryStage\Documentation" -Recurse
Copy-Item -LiteralPath "$projectRoot\docs\validation" -Destination "$binaryStage\Validation" -Recurse
Protect-PackageDocumentation (Join-Path $binaryStage 'Validation')
Copy-Item -LiteralPath "$distPath\$packageBase-source.zip" -Destination $binaryStage
$binarySignature = Get-AuthenticodeSignature -LiteralPath "$binaryStage\EMBERFOX.vst3\Contents\x86_64-win\EMBERFOX.vst3"
$standaloneSignature = Get-AuthenticodeSignature -LiteralPath "$binaryStage\EMBERFOX.exe"
$signingStatus = if ($binarySignature.Status -eq 'Valid' -and $binarySignature.TimeStamperCertificate -and $standaloneSignature.Status -eq 'Valid' -and $standaloneSignature.TimeStamperCertificate) {'signed'} else {'unsigned-preview'}
@{version=$versionMatch.Groups[1].Value;publisher='EmberSoft';status=$signingStatus;license='AGPL-3.0-only';supportContact=$null} | ConvertTo-Json | Set-Content -LiteralPath "$binaryStage\release-status.json"
Compress-Archive -Path "$binaryStage\*" -DestinationPath "$distPath\$packageBase-windows-x64.zip" -Force
Copy-Item -LiteralPath $pluginSource -Destination $distPath -Recurse -Force
Copy-Item -LiteralPath "$projectRoot\build\msvc\Emberfox_artefacts\Release\Standalone\EMBERFOX.exe" -Destination $distPath -Force
Get-FileHash -LiteralPath "$distPath\$packageBase-windows-x64.zip","$distPath\$packageBase-source.zip","$distPath\EMBERFOX.vst3\Contents\x86_64-win\EMBERFOX.vst3" -Algorithm SHA256 |
    Format-Table -AutoSize | Out-String -Width 300 | Set-Content -LiteralPath "$distPath\SHA256SUMS.txt"
# Delete only this invocation's verified staging directory inside this project's build folder.
$absoluteStage = [IO.Path]::GetFullPath($stagePath)
$absoluteBuild = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build')) + [IO.Path]::DirectorySeparatorChar
if (-not $absoluteStage.StartsWith($absoluteBuild, [StringComparison]::OrdinalIgnoreCase)) { throw 'Staging path escaped the project build directory.' }
Remove-Item -LiteralPath $absoluteStage -Recurse -Force
Write-Output "Packages created in $distPath"
