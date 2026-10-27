param([string]$Configuration='Release')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$version=[regex]::Match((Get-Content -LiteralPath "$projectRoot\CMakeLists.txt" -Raw),'project\(Emberfox\s+VERSION\s+([0-9.]+)').Groups[1].Value
if(-not $version){throw 'Project version missing.'}
$checks=[Collections.Generic.List[string]]::new()
foreach($relative in @("VST3\EMBERFOX.vst3\Contents\x86_64-win\EMBERFOX.vst3",'Standalone\EMBERFOX.exe')){
    $file=Join-Path $projectRoot "build\msvc\Emberfox_artefacts\$Configuration\$relative"
    $info=(Get-Item -LiteralPath $file).VersionInfo
    if($info.FileVersion -ne $version -or $info.ProductVersion -ne $version -or $info.CompanyName -ne 'EmberSoft'){
        throw "Windows metadata mismatch: $relative file=$($info.FileVersion), product=$($info.ProductVersion), company=$($info.CompanyName); expected $version / EmberSoft."
    }
    $checks.Add("PASS Windows file/product version $version and publisher EmberSoft: $relative")
}
$checks | Set-Content -LiteralPath "$projectRoot\docs\validation\release-metadata-results.txt"
$checks
