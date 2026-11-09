Set-StrictMode -Version Latest

function New-WindowsSigningContext {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$SignToolPath,
        [Parameter(Mandatory)][string]$CertificateThumbprint,
        [ValidateSet('CurrentUser', 'LocalMachine')][string]$CertificateStore = 'CurrentUser',
        [Parameter(Mandatory)][uri]$TimestampUrl
    )
    if (-not (Test-Path -LiteralPath $SignToolPath -PathType Leaf)) { throw 'SignTool executable not found.' }
    if ($TimestampUrl.Scheme -ne 'https') { throw 'An HTTPS RFC3161 timestamp endpoint is required.' }
    $thumbprint = $CertificateThumbprint.Replace(' ', '').ToUpperInvariant()
    if ($thumbprint -notmatch '^[0-9A-F]{40}$') { throw 'Expected an exact SHA1 certificate thumbprint (40 hex characters).' }
    $cert = Get-Item -LiteralPath "Cert:\$CertificateStore\My\$thumbprint" -ErrorAction Stop
    if (-not $cert.HasPrivateKey) { throw 'Signing identity has no accessible private key.' }
    if ($cert.NotBefore -gt (Get-Date) -or $cert.NotAfter -le (Get-Date)) { throw 'Signing certificate is outside its validity period.' }
    if ('1.3.6.1.5.5.7.3.3' -notin @($cert.EnhancedKeyUsageList | ForEach-Object { $_.ObjectId.Value })) {
        throw 'Certificate must explicitly allow Code Signing.'
    }
    [pscustomobject]@{ Tool = (Resolve-Path -LiteralPath $SignToolPath).Path; Thumbprint = $thumbprint; Store = $CertificateStore; Timestamp = $TimestampUrl.AbsoluteUri }
}

function Test-WindowsArtifactSignature {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$SignToolPath,
        [string]$ExpectedThumbprint
    )
    $signature = Get-AuthenticodeSignature -LiteralPath $Path -ErrorAction Stop
    if ($signature.Status -ne 'Valid') { throw "Signature is not trusted/valid: $Path ($($signature.Status))" }
    if ($null -eq $signature.TimeStamperCertificate) { throw "Missing trusted timestamp: $Path" }
    if ($ExpectedThumbprint -and $signature.SignerCertificate.Thumbprint -ne $ExpectedThumbprint.Replace(' ', '')) {
        throw "Unexpected publisher certificate: $Path"
    }
    # ODBC is user-mode code: use Authenticode policy, not kernel-driver policy.
    & $SignToolPath verify /pa /all /tw $Path | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "SignTool verification failed: $Path (exit $LASTEXITCODE)" }
    [pscustomobject]@{
        name = [IO.Path]::GetFileName($Path)
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
        signer = $signature.SignerCertificate.Subject
        thumbprint = $signature.SignerCertificate.Thumbprint
        timestamper = $signature.TimeStamperCertificate.Subject
    }
}

function Set-WindowsArtifactSignature {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)]$Context,
        [switch]$PreserveTrustedSignature
    )
    if ([IO.Path]::GetExtension($Path) -notin '.dll', '.exe', '.msi') { throw "Unsupported signing target: $Path" }
    $signature = Get-AuthenticodeSignature -LiteralPath $Path -ErrorAction Stop
    if ($PreserveTrustedSignature -and $signature.Status -eq 'Valid') {
        return Test-WindowsArtifactSignature -Path $Path -SignToolPath $Context.Tool
    }
    # Never cover up an invalid existing signature; investigate the input instead.
    if ($signature.Status -notin 'NotSigned', 'Valid') { throw "Refusing to overwrite invalid signature: $Path ($($signature.Status))" }
    $arguments = @('sign', '/sha1', $Context.Thumbprint, '/s', 'My', '/fd', 'SHA256', '/tr', $Context.Timestamp, '/td', 'SHA256')
    if ($Context.Store -eq 'LocalMachine') { $arguments += '/sm' }
    & $Context.Tool @arguments $Path | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $Path (exit $LASTEXITCODE)" }
    Test-WindowsArtifactSignature -Path $Path -SignToolPath $Context.Tool -ExpectedThumbprint $Context.Thumbprint
}

Export-ModuleMember -Function New-WindowsSigningContext, Set-WindowsArtifactSignature, Test-WindowsArtifactSignature
