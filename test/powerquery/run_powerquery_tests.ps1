[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DataConnectorsDir,

    [Parameter(Mandatory = $true)]
    [string]$PQTestExe,

    [Parameter(Mandatory = $true)]
    [string]$ExtensionPath,

    [switch]$ValidateQueryFolding,
    [switch]$FailOnFoldingFailure
)

$ErrorActionPreference = 'Stop'

$frameworkRoot = (Resolve-Path -LiteralPath $DataConnectorsDir).Path
$pqTestPath = (Resolve-Path -LiteralPath $PQTestExe).Path
$extension = (Resolve-Path -LiteralPath $ExtensionPath).Path
$testsDir = Join-Path $frameworkRoot 'testframework\tests'
$runner = Join-Path $testsDir 'RunPQSDKTestSuites.ps1'

if (!(Test-Path -LiteralPath $runner)) {
    throw "Power Query SDK Test Framework runner not found: $runner"
}

$expectedCommit = 'c7b9d81d0d1a62b5f5486f63087c8587e2ca0160'
$actualCommit = (& git -C $frameworkRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "DataConnectors must be checked out at $expectedCommit; got $actualCommit."
}

$connectorConfig = Join-Path $testsDir 'ConnectorConfigs\MatrixOne'
$settingsDir = Join-Path $connectorConfig 'Settings'
$parameterDir = Join-Path $connectorConfig 'ParameterQueries'
$diagnosticsDir = $null
New-Item -ItemType Directory -Force -Path $settingsDir, $parameterDir | Out-Null

Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SanitySettings.json') `
    -Destination $settingsDir -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'StandardSettings.json') `
    -Destination $settingsDir -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'MatrixOne.parameterquery.pq') `
    -Destination $parameterDir -Force

Push-Location $testsDir
try {
    if ($FailOnFoldingFailure -or $ValidateQueryFolding) {
        # PQTest treats existing .diagnostics files as golden command text.
        # Reusing the functional-test output makes strict folding report false
        # failures whenever the current folded SQL differs from a previous run.
        # Functional folding validation also needs fresh diagnostics: otherwise
        # a previous connector's SQL is compared as a golden expectation.
        # Give each invocation an empty, private diagnostics directory.
        $diagnosticsDir = Join-Path ([System.IO.Path]::GetTempPath()) (
            'matrixone-powerquery-diagnostics-{0}-{1}' -f $PID,
            [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $diagnosticsDir | Out-Null
        $failed = 0
        foreach ($settings in 'SanitySettings.json', 'StandardSettings.json') {
            $settingsPath = Join-Path $settingsDir $settings
            $foldingArguments = @()
            if ($FailOnFoldingFailure) { $foldingArguments += '--failOnFoldingFailure' }
            $rawResult = & $pqTestPath compare -p -e $extension `
                -sf $settingsPath -dfp $diagnosticsDir @foldingArguments 2>&1
            $processExitCode = $LASTEXITCODE
            $rawResult | Write-Output

            $testResults = @()
            try {
                $testResults = ($rawResult -join [Environment]::NewLine) |
                    ConvertFrom-Json
                $testFailures = @($testResults | Where-Object {
                    $_.Status -ne 'Passed'
                })
                if (@($testResults).Count -eq 0) { throw 'PQTest returned no test results.' }
            } catch {
                Write-Warning "Could not parse PQTest output for $settings`: $_"
                $testFailures = @('unparseable output')
            }

            $mode = if ($FailOnFoldingFailure) { 'STRICT' } else { 'FUNCTIONAL' }
            Write-Host ("{0} {1}: total={2} failed={3} exit={4}" -f `
                $mode, $settings, @($testResults).Count, $testFailures.Count,
                $processExitCode)
            if ($processExitCode -ne 0 -or $testFailures.Count -ne 0) {
                $failed++
            }
        }
        if ($failed -ne 0) {
            throw "$failed $mode test suite(s) failed."
        }
    } else {
        $arguments = @{
            PQTestExePath = $pqTestPath
            ExtensionPath = $extension
            TestSettingsDirectoryPath = $settingsDir
            TestSettingsList = @('SanitySettings.json', 'StandardSettings.json')
            DetailedResults = $true
            JSONResults = $true
            Silent = $true
        }
        if ($ValidateQueryFolding) {
            $arguments.ValidateQueryFolding = $true
        }
        & $runner @arguments
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
} finally {
    Pop-Location
    if ($null -ne $diagnosticsDir -and
        (Test-Path -LiteralPath $diagnosticsDir)) {
        Remove-Item -LiteralPath $diagnosticsDir -Recurse -Force
    }
}
