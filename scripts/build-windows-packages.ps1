[CmdletBinding()]
param(
    [ValidateRange(1, 99)]
    [int]$PackageRevision = 4,
    [ValidateSet('Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo',
    [Parameter(Mandatory)]
    [string]$MySqlDir,
    [Parameter(Mandatory)]
    [string]$WixDir,
    [string]$BuildDirectory = "$PSScriptRoot\..\build-windows-package",
    [string]$OutputDirectory = "$PSScriptRoot\..\artifacts\windows-package",
    [switch]$RequireSigning,
    [string]$SignToolPath,
    [string]$CertificateThumbprint,
    [ValidateSet('CurrentUser', 'LocalMachine')]
    [string]$CertificateStore = 'CurrentUser',
    [string]$TimestampUrl
)

$ErrorActionPreference = 'Stop'
$signing = $null
if ($RequireSigning) {
    if (-not $SignToolPath -or -not $CertificateThumbprint -or -not $TimestampUrl) {
        throw 'Signed packages require SignToolPath, CertificateThumbprint and TimestampUrl; unsigned fallback is forbidden.'
    }
    Import-Module (Join-Path $PSScriptRoot 'WindowsSigning.psm1') -Force
    $signing = New-WindowsSigningContext -SignToolPath $SignToolPath `
        -CertificateThumbprint $CertificateThumbprint -CertificateStore $CertificateStore -TimestampUrl $TimestampUrl
} else {
    Write-Warning 'UNSIGNED DEVELOPMENT BUILD: not approved for customer release.'
}
$source = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = [System.IO.Path]::GetFullPath($BuildDirectory)
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$mysql = [System.IO.Path]::GetFullPath($MySqlDir)
$wix = [System.IO.Path]::GetFullPath($WixDir)

foreach ($path in @(
    (Join-Path $mysql 'include\mysql.h'),
    (Join-Path $mysql 'lib\libmysql.lib'),
    (Join-Path $wix 'wix.exe')
)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required build input is missing: $path"
    }
}
if (Test-Path -LiteralPath $build) {
    throw "Build directory must not already exist: $build"
}
if ((Test-Path -LiteralPath $output) -and @(Get-ChildItem -LiteralPath $output -Force).Count -gt 0) {
    throw "Output directory must be empty to avoid stale release assets: $output"
}
New-Item -ItemType Directory -Path $build, $output -Force | Out-Null

$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 `
    "-DMYSQL_DIR=$mysql" `
    '-DMYSQLCLIENT_STATIC_LINKING=0' `
    '-DBUNDLE_DEPENDENCIES=1' `
    '-DWITH_PBI=1' `
    "-DMATRIXONE_PACKAGE_REVISION=$PackageRevision"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $LASTEXITCODE" }

$packageVersion = "9.7.0-mo.$PackageRevision"
$archiveRoot = Join-Path $build "matrixone-odbc-$packageVersion-winx64"
New-Item -ItemType Directory -Path $archiveRoot | Out-Null
foreach ($component in 'OdbcDll', 'ODBCDll', 'OdbcUtil', 'Readme', 'Unspecified') {
    & $cmake --install $build --config $Configuration --prefix $archiveRoot --component $component
    if ($LASTEXITCODE -ne 0) { throw "Install component $component failed: $LASTEXITCODE" }
}
$pbiTarget = New-Item -ItemType Directory -Force -Path (Join-Path $archiveRoot 'pbi')
Copy-Item -LiteralPath (Join-Path $build 'pbi\MatrixOne.mez') -Destination $pbiTarget.FullName
Copy-Item -LiteralPath (Join-Path $source 'docs') -Destination $archiveRoot -Recurse
Get-ChildItem $archiveRoot -Recurse -File -Filter '*.pdb' |
    Remove-Item -Force
$sdkFiles = @(Get-ChildItem $archiveRoot -Recurse -File -Include '*.h', '*.hpp', '*.lib')
if ($sdkFiles) {
    throw "Runtime ZIP contains SDK files: $($sdkFiles.FullName -join ', ')"
}

$signatureRecords = @()
if ($RequireSigning) {
    $binaries = @(Get-ChildItem -LiteralPath $archiveRoot -Recurse -File | Where-Object Extension -In '.dll', '.exe')
    if ($binaries.Count -eq 0) { throw 'No runtime binaries found to sign.' }
    foreach ($binary in $binaries) {
        # Preserve valid upstream signatures; MatrixOne-built myodbc* files must
        # carry the explicitly selected release identity.
        $record = Set-WindowsArtifactSignature -Path $binary.FullName -Context $signing `
            -PreserveTrustedSignature:($binary.Name -notlike 'myodbc*')
        $record | Add-Member -NotePropertyName relativePath -NotePropertyValue ($binary.FullName.Substring($archiveRoot.Length + 1))
        $signatureRecords += $record
    }
}

$archive = Join-Path $output "matrixone-odbc-$packageVersion-winx64.zip"
Compress-Archive -LiteralPath $archiveRoot -DestinationPath $archive -CompressionLevel Optimal

$wixBuild = Join-Path $build 'wix-package'
$x64 = Join-Path $wixBuild 'x64'
$plugin = Join-Path $x64 'plugin'
$doc = Join-Path $wixBuild 'doc'
$pbi = Join-Path $wixBuild 'pbi'
New-Item -ItemType Directory -Path $x64, $plugin, $doc, $pbi -Force | Out-Null
Copy-Item -Path (Join-Path $archiveRoot 'lib\*.dll'), (Join-Path $archiveRoot 'lib\*.exe') -Destination $x64
Copy-Item -Path (Join-Path $archiveRoot 'lib\plugin\*.dll') -Destination $plugin
Copy-Item -LiteralPath (Join-Path $archiveRoot 'bin\myodbc-installer.exe') -Destination $x64
Copy-Item -LiteralPath (Join-Path $archiveRoot 'pbi\MatrixOne.mez') -Destination $pbi
foreach ($file in 'README.txt', 'ChangeLog.txt', 'LICENSE.txt', 'INFO_BIN', 'INFO_SRC') {
    Copy-Item -LiteralPath (Join-Path $archiveRoot $file) -Destination $doc
}

& $cmake -S (Join-Path $source 'wix') -B $wixBuild -G 'Visual Studio 17 2022' -A x64 `
    '-DMSI_64=1' '-DLICENSE=0' "-DWIX_DIR=$wix" `
    "-DWIX_DOCUMENT_DIR=$doc" "-DMATRIXONE_PACKAGE_REVISION=$PackageRevision"
if ($LASTEXITCODE -ne 0) { throw "WiX configure failed: $LASTEXITCODE" }
& $cmake --build $wixBuild --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "WiX build failed: $LASTEXITCODE" }

$msis = @(Get-ChildItem $wixBuild -File -Filter "matrixone-odbc-$packageVersion-winx64.msi")
if ($msis.Count -ne 1) {
    throw "Expected exactly one MSI, found $($msis.Count) in $wixBuild"
}
$msi = $msis[0]
if ($RequireSigning) {
    $signatureRecords += Set-WindowsArtifactSignature -Path $msi.FullName -Context $signing
    # Recheck the final ZIP payload, not merely the staging tree.
    $verifyRoot = Join-Path $build 'verify-signed-zip'
    Expand-Archive -LiteralPath $archive -DestinationPath $verifyRoot
    foreach ($record in @($signatureRecords | Where-Object { $_.PSObject.Properties.Name -contains 'relativePath' })) {
        $packedPath = Join-Path (Join-Path $verifyRoot (Split-Path $archiveRoot -Leaf)) $record.relativePath
        $verified = Test-WindowsArtifactSignature -Path $packedPath -SignToolPath $signing.Tool -ExpectedThumbprint $record.thumbprint
        if ($verified.sha256 -ne $record.sha256) { throw "ZIP payload changed: $packedPath" }
    }
}
Copy-Item -LiteralPath $msi.FullName -Destination $output -Force

[pscustomobject]@{
    signed = [bool]$RequireSigning
    customerReleaseCandidate = [bool]$RequireSigning
    note = 'Signature checks do not replace clean-machine application-control and Power BI acceptance tests.'
    files = $signatureRecords
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'signing-report.json') -Encoding UTF8

if ($RequireSigning) {
    & (Join-Path $PSScriptRoot 'Test-SignedWindowsPackage.ps1') -PackageDirectory $output `
        -SignToolPath $signing.Tool -ExpectedThumbprint $signing.Thumbprint
}

$assets = Get-ChildItem $output -File | Sort-Object Name | ForEach-Object {
    [pscustomobject]@{
        name = $_.Name
        bytes = $_.Length
        sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$assets | ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding UTF8
$assets | Format-Table -AutoSize
