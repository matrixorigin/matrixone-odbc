[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SignToolPath,
    [Parameter(Mandatory)][string]$UnsignedFile,
    [Parameter(Mandatory)][string]$SignedFile
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $repo 'scripts/WindowsSigning.psm1') -Force
$script:passed = 0
function Expect-Failure([string]$Name, [scriptblock]$Action, [string]$Pattern) {
    $message = $null
    try { & $Action | Out-Null } catch { $message = $_.Exception.Message }
    if (-not $message -or $message -notmatch $Pattern) { throw "FAIL ${Name}: unexpected result '$message'" }
    $script:passed++
    Write-Host "PASS $Name"
}
foreach ($path in @(
    'scripts/WindowsSigning.psm1', 'scripts/build-windows-packages.ps1',
    'scripts/Test-SignedWindowsPackage.ps1', 'test/windows/Test-SigningGuards.ps1'
)) {
    $tokens = $null
    $errors = $null
    [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $repo $path), [ref]$tokens, [ref]$errors)
    if ($errors.Count) { throw "Parse errors in ${path}: $errors" }
}
Write-Host 'PASS PowerShell syntax (4 files)'
$script:passed++
$dummy = '0' * 40
Expect-Failure 'Missing signing configuration fails before build' {
    & (Join-Path $repo 'scripts/build-windows-packages.ps1') -MySqlDir unused -WixDir unused -RequireSigning
} 'unsigned fallback is forbidden'
Expect-Failure 'Missing SignTool rejected' {
    New-WindowsSigningContext -SignToolPath (Join-Path $repo 'missing-signtool.exe') -CertificateThumbprint $dummy -TimestampUrl https://example.invalid
} 'SignTool executable not found'
Expect-Failure 'Non-HTTPS timestamp rejected' {
    New-WindowsSigningContext -SignToolPath $SignToolPath -CertificateThumbprint $dummy -TimestampUrl http://example.invalid
} 'HTTPS RFC3161'
Expect-Failure 'Malformed identity rejected' {
    New-WindowsSigningContext -SignToolPath $SignToolPath -CertificateThumbprint invalid -TimestampUrl https://example.invalid
} '40 hex'
Expect-Failure 'Missing certificate rejected' {
    New-WindowsSigningContext -SignToolPath $SignToolPath -CertificateThumbprint $dummy -TimestampUrl https://example.invalid
} 'Cannot find path|does not exist'
Expect-Failure 'Unsigned binary rejected' {
    Test-WindowsArtifactSignature -Path $UnsignedFile -SignToolPath $SignToolPath
} 'Signature is not trusted/valid'
Expect-Failure 'Wrong publisher rejected' {
    Test-WindowsArtifactSignature -Path $SignedFile -SignToolPath $SignToolPath -ExpectedThumbprint $dummy
} 'Unexpected publisher'
Expect-Failure 'MEZ is not a PE signing target' {
    Set-WindowsArtifactSignature -Path 'MatrixOne.mez' -Context @{}
} 'Unsupported signing target'
$before = (Get-FileHash -LiteralPath $SignedFile).Hash
$context = [pscustomobject]@{ Tool = $SignToolPath }
$record = Set-WindowsArtifactSignature -Path $SignedFile -Context $context -PreserveTrustedSignature
if ($record.sha256 -ne $before.ToLowerInvariant() -or (Get-FileHash -LiteralPath $SignedFile).Hash -ne $before) {
    throw 'Preserving third-party signature altered the file.'
}
$script:passed++
Write-Host 'PASS Real third-party signature/timestamp verified without alteration'
Write-Host "PASS $script:passed checks. Actual signing and signed MSI lifecycle require a provisioned signing identity."
