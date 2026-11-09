[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [Parameter(Mandatory)][string]$SignToolPath,
    [Parameter(Mandatory)][string]$ExpectedThumbprint
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'WindowsSigning.psm1') -Force
$report = Get-Content -LiteralPath (Join-Path $PackageDirectory 'signing-report.json') -Raw | ConvertFrom-Json
if (-not $report.signed) { throw 'Unsigned development package is not a signed release.' }
$msis = @(Get-ChildItem -LiteralPath $PackageDirectory -Filter '*.msi' -File)
$zips = @(Get-ChildItem -LiteralPath $PackageDirectory -Filter '*.zip' -File)
if ($msis.Count -ne 1 -or $zips.Count -ne 1) { throw 'Expected one MSI and one ZIP.' }
Test-WindowsArtifactSignature -Path $msis[0].FullName -SignToolPath $SignToolPath -ExpectedThumbprint $ExpectedThumbprint
$work = Join-Path ([IO.Path]::GetTempPath()) ('mo-signed-package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
# Keep extracted evidence for inspection; do not install or register the driver.
$zipRoot = Join-Path $work 'zip'
$msiRoot = Join-Path $work 'msi'
Expand-Archive -LiteralPath $zips[0].FullName -DestinationPath $zipRoot
$arguments = '/a "{0}" /qn TARGETDIR="{1}" /l*v "{2}"' -f $msis[0].FullName, $msiRoot, (Join-Path $work 'extract.log')
$process = Start-Process -FilePath msiexec.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -notin 0,3010) { throw "MSI extraction failed: $($process.ExitCode). Evidence: $work" }
foreach ($root in $zipRoot, $msiRoot) {
    $binaries = @(Get-ChildItem -LiteralPath $root -File -Recurse | Where-Object Extension -In '.dll', '.exe')
    if ($binaries.Count -eq 0) { throw "No binaries in extracted package: $root" }
    $actual = @()
    foreach ($binary in $binaries) {
        $expected = if ($binary.Name -like 'myodbc*') { $ExpectedThumbprint } else { $null }
        $actual += Test-WindowsArtifactSignature -Path $binary.FullName -SignToolPath $SignToolPath -ExpectedThumbprint $expected
    }
    $payload = @($report.files | Where-Object { $_.PSObject.Properties.Name -contains 'relativePath' })
    foreach ($record in $payload) {
        if (-not @($actual | Where-Object { $_.name -eq $record.name -and $_.sha256 -eq $record.sha256 }).Count) {
            throw "Signed payload missing or altered in ${root}: $($record.name)"
        }
    }
    foreach ($record in $actual) {
        if (-not @($payload | Where-Object { $_.name -eq $record.name -and $_.sha256 -eq $record.sha256 }).Count) {
            throw "Unrecorded executable in ${root}: $($record.name)"
        }
    }
}
Write-Host "PASS: MSI and ZIP signatures, timestamps, publishers and payload hashes. Evidence: $work"
