[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('MsiLifecycle', 'PortableZip')]
    [string]$Mode,
    [Parameter(Mandatory)]
    [string]$Package,
    [string]$PreviousPackage,
    [string]$OracleMsi,
    [string]$Server = '',
    [int]$Port = 6001,
    [string]$User = 'root',
    [string]$Password = '111',
    [string]$Artifacts = "$PSScriptRoot\..\..\artifacts\windows-package"
)

$ErrorActionPreference = 'Stop'
$driverNames = @(
    'MatrixOne ODBC 9.7 Unicode Driver',
    'MatrixOne ODBC 9.7 ANSI Driver'
)
$testInstalled = Join-Path $PSScriptRoot 'Test-InstalledDriver.ps1'
$connect = -not [string]::IsNullOrWhiteSpace($Server)
$results = [System.Collections.Generic.List[object]]::new()
New-Item -ItemType Directory -Force -Path $Artifacts | Out-Null

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Add-Result([string]$Scenario, [string]$Status, [string]$Detail) {
    $results.Add([pscustomobject]@{ scenario=$Scenario; status=$Status; detail=$Detail })
}

function Invoke-NativeProcess([string]$FilePath, [string[]]$Arguments, [int]$TimeoutSeconds = 120) {
    Assert-True (-not ($Arguments | Where-Object { $_ -match '["\r\n]' })) 'Native argument contains an unsupported quote or newline.'
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.UseShellExecute = $false
    foreach ($argument in $Arguments) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    $process = [System.Diagnostics.Process]::Start($startInfo)
    Assert-True ($null -ne $process) "Failed to start $FilePath."
    try {
        if (-not $process.WaitForExit(5000)) {
            $commandLine = Get-CimInstance Win32_Process -Filter "ProcessId=$($process.Id)" -ErrorAction SilentlyContinue
            Write-Host "Still running: $($commandLine.CommandLine)"
            Get-Service msiserver -ErrorAction SilentlyContinue | Format-List Status, StartType
        }
        if (-not $process.HasExited -and -not $process.WaitForExit(($TimeoutSeconds - 5) * 1000)) {
            $process.Kill($true)
            $process.WaitForExit()
            $logArgument = $Arguments | Where-Object { $_ -like '*.log' } | Select-Object -Last 1
            if ($logArgument -and (Test-Path -LiteralPath $logArgument)) {
                Get-Content -LiteralPath $logArgument -Tail 100
            }
            throw "$FilePath timed out after $TimeoutSeconds seconds."
        }
        return $process.ExitCode
    } finally { [void]$process.Dispose() }
}

function Invoke-Msi([string[]]$Arguments, [string]$LogName, [int[]]$AllowedExitCodes = @(0, 3010)) {
    $log = Join-Path $Artifacts $LogName
    $msiArguments = @($Arguments) + @('/qn', '/norestart', '/l*v', $log)
    Write-Host "Starting MSI scenario $LogName"
    $exitCode = Invoke-NativeProcess -FilePath msiexec.exe -Arguments $msiArguments
    Write-Host "Completed MSI scenario $LogName with exit code $exitCode"
    Assert-True ($exitCode -in $AllowedExitCodes) "msiexec exit $exitCode; see $log"
    return $exitCode
}

function Get-MsiProperty([string]$Path, [string]$Name) {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $database = $null
    $view = $null
    $record = $null
    $value = $null
    try {
        $database = $installer.OpenDatabase($Path, 0)
        $view = $database.OpenView("SELECT ``Value`` FROM ``Property`` WHERE ``Property``='$Name'")
        [void]$view.Execute()
        $record = $view.Fetch()
        if ($record) { $value = $record.StringData(1) }
    } finally {
        if ($view) { [void]$view.Close() }
        foreach ($comObject in @($record, $view, $database, $installer)) {
            if ($comObject) {
                [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($comObject)
            }
        }
    }
    return $value
}

function Get-DriverPath([string]$Name) {
    (Get-ItemProperty -LiteralPath "HKLM:\SOFTWARE\ODBC\ODBCINST.INI\$Name").Driver
}

function Test-Connection {
    if ([string]::IsNullOrWhiteSpace($Server)) { return }
    & $testInstalled -Server $Server -Port $Port -User $User -Password $Password -RequireConnection
}

function Remove-TestDsn {
    Remove-OdbcDsn -Name 'MatrixOnePackageTest' -DsnType System -Platform '64-bit' -ErrorAction SilentlyContinue
}

try {
    if ($Mode -eq 'PortableZip') {
        $destination = Join-Path $env:TEMP "MatrixOne ODBC portable 路径 $([guid]::NewGuid()) with spaces"
        Expand-Archive -LiteralPath $Package -DestinationPath $destination -Force
        $root = Get-ChildItem $destination -Directory | Select-Object -First 1 -ExpandProperty FullName
        Assert-True (-not (Get-ChildItem $root -Recurse -File -Include *.h,*.hpp,*.lib)) 'Portable package contains SDK headers or import libraries.'
        $installer = Join-Path $root 'bin\myodbc-installer.exe'
        $lib = Join-Path $root 'lib'
        foreach ($tuple in @(@('ANSI','myodbc9a.dll'), @('Unicode','myodbc9w.dll'))) {
            $name = "MatrixOne ODBC 9.7 $($tuple[0]) Driver"
            & $installer -d -a -n $name -t "DRIVER=$lib\$($tuple[1]);SETUP=$lib\myodbc9S.dll"
            Assert-True ($LASTEXITCODE -eq 0) "Portable driver registration failed: $name"
        }
        & $testInstalled -Server $Server -Port $Port -User $User -Password $Password -RequireConnection:$connect
        Add-Result 'portable ZIP/custom Unicode path/no SDK' PASS $root

        $saved = Join-Path $lib 'libmysql.dll.saved'
        Move-Item -LiteralPath (Join-Path $lib 'libmysql.dll') -Destination $saved
        try {
            $failed = $false
            try { & $testInstalled } catch { $failed = $true }
            Assert-True $failed 'Connection unexpectedly succeeded without libmysql.dll.'
        } finally { Move-Item -LiteralPath $saved -Destination (Join-Path $lib 'libmysql.dll') }
        & $testInstalled -Server $Server -Port $Port -User $User -Password $Password -RequireConnection:$connect
        Add-Result 'missing bundled runtime fails and recovers' PASS 'libmysql.dll restored'

        foreach ($name in $driverNames) {
            & $installer -d -r -n $name
            Assert-True ($LASTEXITCODE -eq 0) "Portable driver removal failed: $name"
        }
        Assert-True (-not (Get-OdbcDriver -Platform '64-bit' | Where-Object Name -In $driverNames)) 'Portable drivers remain registered.'
        Add-Result 'portable uninstall' PASS 'registry removed'
    } else {
        Assert-True (Test-Path $Package) "Package not found: $Package"
        $productCode = Get-MsiProperty $Package ProductCode
        $productVersion = Get-MsiProperty $Package ProductVersion
        Assert-True ($productVersion -match '^9\.7\.[1-9][0-9]?$') "Downstream MSI revision missing from ProductVersion: $productVersion"

        $installDir = Join-Path $env:ProgramFiles 'Matrix Origin\MatrixOne ODBC install path'
        Invoke-Msi @('/i', $Package, "INSTALLDIR=$installDir") '01-clean-install.log' | Out-Null
        & $testInstalled -Server $Server -Port $Port -User $User -Password $Password -RequireConnection:$connect
        Assert-True ((Get-DriverPath $driverNames[0]).StartsWith($installDir, [StringComparison]::OrdinalIgnoreCase)) 'Custom INSTALLDIR was ignored.'
        Assert-True (-not (Get-ChildItem $installDir -Recurse -File -Include *.h,*.hpp,*.lib)) 'MSI install contains SDK headers or import libraries.'
        Add-Result 'clean MSI/custom path/no MySQL SDK' PASS "$productVersion $productCode"

        Add-OdbcDsn -Name 'MatrixOnePackageTest' -DriverName $driverNames[0] -DsnType System -Platform '64-bit' -SetPropertyValue @("SERVER=$Server", "PORT=$Port", 'UID=root', 'PWD=111')
        Invoke-Msi @('/fa', $Package, 'REINSTALL=ALL', 'REINSTALLMODE=vomus') '02-repair.log' | Out-Null
        Assert-True (Get-OdbcDsn -Name 'MatrixOnePackageTest' -DsnType System -Platform '64-bit') 'Repair deleted the system DSN.'
        Test-Connection
        Add-Result 'silent repeat/repair keeps DSN' PASS 'DSN present'

        if ($PreviousPackage) {
            Remove-TestDsn
            Invoke-Msi @('/x', $productCode) '03-remove-current-before-upgrade.log' | Out-Null
            Invoke-Msi @('/i', $PreviousPackage) '04-install-previous.log' | Out-Null
            Add-OdbcDsn -Name 'MatrixOnePackageTest' -DriverName $driverNames[0] -DsnType System -Platform '64-bit' -SetPropertyValue @("SERVER=$Server", "PORT=$Port", 'UID=root', 'PWD=111')
            Invoke-Msi @('/i', $Package) '05-major-upgrade.log' | Out-Null
            Assert-True (Get-OdbcDsn -Name 'MatrixOnePackageTest' -DsnType System -Platform '64-bit') 'Major upgrade deleted the system DSN.'
            Assert-True ((Get-MsiProperty $Package ProductCode) -ne (Get-MsiProperty $PreviousPackage ProductCode)) 'Upgrade packages share ProductCode.'
            Test-Connection
            Add-Result 'mo.1 to mo.2 upgrade keeps DSN' PASS 'major upgrade succeeded'

            $downgradeLog = Join-Path $Artifacts '06-downgrade-blocked.log'
            $downgradeExitCode = Invoke-NativeProcess -FilePath msiexec.exe -Arguments @('/i', $PreviousPackage, '/qn', '/norestart', '/l*v', $downgradeLog)
            Assert-True ($downgradeExitCode -ne 0) 'Downgrade unexpectedly succeeded.'
            & $testInstalled -Server $Server -Port $Port -User $User -Password $Password -RequireConnection:$connect
            Add-Result 'downgrade blocked' PASS "exit $downgradeExitCode"
        }

        if ($OracleMsi) {
            Invoke-Msi @('/i', $OracleMsi) '07-oracle-side-by-side.log' | Out-Null
            $oracle = @(Get-OdbcDriver -Platform '64-bit' | Where-Object Name -Like 'MySQL ODBC*')
            Assert-True ($oracle.Count -ge 2) 'Oracle MySQL ODBC drivers were not installed.'
            & $testInstalled
            Invoke-Msi @('/x', (Get-MsiProperty $OracleMsi ProductCode)) '08-oracle-remove.log' | Out-Null
            & $testInstalled
            Add-Result 'Oracle MySQL ODBC side by side' PASS "$($oracle.Count) Oracle drivers"
        }

        Remove-TestDsn
        $installedRoot = Split-Path -Parent (Get-DriverPath $driverNames[0])
        Invoke-Msi @('/x', $productCode) '09-uninstall.log' | Out-Null
        Assert-True (-not (Get-OdbcDriver -Platform '64-bit' | Where-Object Name -In $driverNames)) 'Drivers remain after uninstall.'
        Assert-True (-not (Test-Path -LiteralPath $installedRoot)) "Install directory remains after uninstall: $installedRoot"

        Invoke-Msi @('/i', $Package) '10-reinstall.log' | Out-Null
        & $testInstalled -Server $Server -Port $Port -User $User -Password $Password -RequireConnection:$connect
        $reinstalledRoot = Split-Path -Parent (Get-DriverPath $driverNames[0])
        Invoke-Msi @('/x', $productCode) '11-final-uninstall.log' | Out-Null
        Assert-True (-not (Get-OdbcDriver -Platform '64-bit' | Where-Object Name -In $driverNames)) 'Drivers remain after the final uninstall.'
        Assert-True (-not (Test-Path -LiteralPath $reinstalledRoot)) "Reinstalled directory remains after uninstall: $reinstalledRoot"
        Add-Result 'complete uninstall and reinstall' PASS 'files and registry removed twice'
    }
} finally {
    Remove-TestDsn
    $results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Artifacts 'summary.json') -Encoding UTF8
    $results | Format-Table -AutoSize
}
