param([string]$Configuration='Release',[ValidateRange(1,10)][int]$StrictnessLevel=10,
      [switch]$SkipPluginval,[switch]$SkipExternalValidation,[string]$SteinbergValidatorPath='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$reportPath=Join-Path $projectRoot 'docs\validation'
New-Item -ItemType Directory -Path $reportPath -Force | Out-Null
$vswherePath=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswherePath -latest -products '*' -property installationPath
$ctestPath=Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $ctestPath --test-dir "$projectRoot\build\msvc" -C $Configuration --output-on-failure 2>&1 | Tee-Object -FilePath "$reportPath\ctest-results.txt"
if($LASTEXITCODE -ne 0){throw 'CTest failed; inspect docs/validation/ctest-results.txt.'}
Copy-Item -LiteralPath "$projectRoot\build\msvc\Testing\Temporary\LastTest.log" -Destination "$reportPath\ctest-detail.txt" -Force
& "$PSScriptRoot\test-release-metadata.ps1" -Configuration $Configuration
if($SkipExternalValidation){Write-Output 'Developer-only run: external validators explicitly skipped.';return}
$pluginPath=Join-Path $projectRoot "build\msvc\Emberfox_artefacts\$Configuration\VST3\EMBERFOX.vst3"
if(-not $SteinbergValidatorPath){$SteinbergValidatorPath=Join-Path $projectRoot 'build\steinberg-validator\bin\Release\validator.exe'}
if(-not(Test-Path -LiteralPath $SteinbergValidatorPath)){throw 'Steinberg validator missing. Run scripts/bootstrap-release-tools.ps1.'}
$steinberg=Start-Process -FilePath $SteinbergValidatorPath -ArgumentList @(('"'+$pluginPath+'"')) -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput "$reportPath\steinberg-output.txt" -RedirectStandardError "$reportPath\steinberg-error.txt"
if($steinberg.ExitCode -ne 0){throw "Steinberg validator failed with exit $($steinberg.ExitCode)."}
$steinbergLog=Get-Content -LiteralPath "$reportPath\steinberg-output.txt" -Raw
if($steinbergLog -notmatch 'Result:\s+\d+ tests passed, 0 tests failed'){throw 'Steinberg validator produced no successful test summary.'}
Get-Content -LiteralPath "$reportPath\steinberg-output.txt" -Tail 4
if($SkipPluginval){Write-Output 'Developer-only run: pluginval explicitly skipped.';return}
$pluginvalPath=Join-Path $projectRoot 'tools\pluginval\pluginval.exe'
if(-not(Test-Path -LiteralPath $pluginvalPath)){throw 'pluginval missing. Install official pluginval into tools/pluginval; release validation cannot skip it.'}
$outputFile=Join-Path $reportPath "pluginval-level$StrictnessLevel-output.txt"
$errorFile=Join-Path $reportPath "pluginval-level$StrictnessLevel-error.txt"
$validator=Start-Process -FilePath $pluginvalPath -ArgumentList @('--headless','--strictness-level',"$StrictnessLevel",'--random-seed','73821','--timeout-ms','120000','--vst3validator',('"'+$SteinbergValidatorPath+'"'),'--validate',('"'+$pluginPath+'"')) -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $outputFile -RedirectStandardError $errorFile
Copy-Item -LiteralPath $outputFile -Destination "$reportPath\pluginval-output.txt" -Force
Copy-Item -LiteralPath $errorFile -Destination "$reportPath\pluginval-error.txt" -Force
$validationLog=Get-Content -LiteralPath $outputFile -Raw
Get-Content -LiteralPath $outputFile -Tail 5
if($validator.ExitCode -ne 0 -or $validationLog -notmatch '(?m)^SUCCESS\s*$'){throw "pluginval level$StrictnessLevel failed (exit $($validator.ExitCode))."}
if($validationLog -match 'Skipping vst3 validator'){throw 'pluginval skipped its integrated Steinberg check; release validation failed.'}
