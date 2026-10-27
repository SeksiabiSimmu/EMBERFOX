param([Parameter(Mandatory)][string[]]$Path,
      [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$Thumbprint,
      [ValidateSet('CurrentUser','LocalMachine')][string]$Store='CurrentUser',
      [string]$TimestampUrl='http://timestamp.digicert.com')
$ErrorActionPreference='Stop'
$certificatePath="Cert:\$Store\My\$Thumbprint"
if(-not(Test-Path -LiteralPath $certificatePath)){throw 'Trusted code-signing certificate not found. No files were signed.'}
$certificate=Get-Item -LiteralPath $certificatePath
if(-not $certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)){throw 'Certificate is expired, not yet valid or lacks its private key.'}
if(@($certificate.EnhancedKeyUsageList | Where-Object {$_.ObjectId.Value -eq '1.3.6.1.5.5.7.3.3'}).Count -eq 0){throw 'Certificate lacks code-signing usage.'}
$chain=[Security.Cryptography.X509Certificates.X509Chain]::new()
try{if(-not $chain.Build($certificate)){throw 'Certificate chain is not trusted. Self-signed release certificates are not accepted.'}}finally{$chain.Dispose()}
$sdkBin=Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" | Sort-Object FullName -Descending | Select-Object -First 1
if(-not $sdkBin){throw 'Install the Windows SDK SignTool.'}
foreach($target in $Path){
    $absoluteTarget=(Resolve-Path -LiteralPath $target).Path
    $arguments=@('sign','/sha1',$Thumbprint,'/s','My','/fd','SHA256','/tr',$TimestampUrl,'/td','SHA256')
    if($Store -eq 'LocalMachine'){$arguments+='/sm'}
    & $sdkBin.FullName @arguments $absoluteTarget
    if($LASTEXITCODE -ne 0){throw "SignTool signing failed: $absoluteTarget"}
    & $sdkBin.FullName verify /pa /all /v $absoluteTarget
    if($LASTEXITCODE -ne 0){throw "SignTool verification failed: $absoluteTarget"}
    $signature=Get-AuthenticodeSignature -LiteralPath $absoluteTarget
    if($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate){throw 'A valid timestamped signature is required.'}
}
