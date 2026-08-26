param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot
)

$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]$identity
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'This regression test must run from a non-elevated process.'
}

$package = (Resolve-Path -LiteralPath $PackageRoot).Path
$installer = Join-Path $package 'bin\myodbc-installer.exe'
$lib = Join-Path $package 'lib'
$name = "MatrixOne ODBC non-admin probe $([Guid]::NewGuid())"
$keys = @(
    "HKLM:\SOFTWARE\ODBC\ODBCINST.INI\$name",
    "HKLM:\SOFTWARE\WOW6432Node\ODBC\ODBCINST.INI\$name",
    "HKCU:\SOFTWARE\ODBC\ODBCINST.INI\$name"
)

foreach ($key in $keys) {
    if (Test-Path -LiteralPath $key) {
        throw "Refusing to overwrite pre-existing driver registration: $key"
    }
}

$output = & $installer -d -a -n $name -t "DRIVER=$(Join-Path $lib 'myodbc9w.dll');SETUP=$(Join-Path $lib 'myodbc9S.dll')" 2>&1
$exitCode = $LASTEXITCODE
$created = @($keys | Where-Object { Test-Path -LiteralPath $_ })

try {
    if ($created.Count -ne 0) {
        throw "Non-admin registration unexpectedly created: $($created -join ', ')"
    }
    if ($exitCode -eq 0) {
        throw "Installer reported success without registering the driver. Output: $($output -join ' | ')"
    }
    Write-Output "PASS non-admin registration failed truthfully (exit $exitCode)"
    Write-Output ($output -join [Environment]::NewLine)
}
finally {
    foreach ($key in $created) {
        Remove-Item -LiteralPath $key -Recurse -Force -ErrorAction SilentlyContinue
    }
}
