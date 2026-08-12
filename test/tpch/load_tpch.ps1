[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MySqlExe,

    [Parameter(Mandatory = $true)]
    [string]$PluginDir,

    [string]$SourceDir = '/root/data/tpch1g/data',
    [string]$Server = 'localhost',
    [int]$Port = 6001,
    [string]$User = 'root',
    [string]$Password = '111'
)

$ErrorActionPreference = 'Stop'

$mysqlPath = (Resolve-Path $MySqlExe).Path
$pluginPath = (Resolve-Path $PluginDir).Path
$schemaPath = Join-Path $PSScriptRoot 'schema.sql'

$baseArgs = @(
    '--protocol=TCP'
    "--host=$Server"
    "--port=$Port"
    "--user=$User"
    '--ssl-mode=DISABLED'
    "--plugin-dir=$pluginPath"
    '--default-character-set=utf8mb4'
    '--batch'
    '--skip-column-names'
)

$tables = [ordered]@{
    nation   = @{ File = 'nation.tbl'; Expected = 25 }
    region   = @{ File = 'region.tbl'; Expected = 5 }
    supplier = @{ File = 'supplier.tbl'; Expected = 10000 }
    customer = @{ File = 'customer.tbl'; Expected = 150000 }
    part     = @{ File = 'part.tbl'; Expected = 200000 }
    partsupp = @{ File = 'partsupp.tbl'; Expected = 800000 }
    orders   = @{ File = 'orders.tbl'; Expected = 1500000 }
    lineitem = @{ File = 'lineitem.tbl'; Expected = 6001215 }
}

$previousPassword = $env:MYSQL_PWD
$env:MYSQL_PWD = $Password

try {
    Write-Host "Creating the TPC-H schema from $schemaPath"
    $schemaProcess = Start-Process -FilePath $mysqlPath -ArgumentList $baseArgs `
        -RedirectStandardInput $schemaPath -NoNewWindow -Wait -PassThru
    if ($schemaProcess.ExitCode -ne 0) {
        throw "TPC-H schema creation failed with exit code $($schemaProcess.ExitCode)."
    }

    foreach ($entry in $tables.GetEnumerator()) {
        $table = $entry.Key
        $file = $entry.Value.File
        $serverPath = "$($SourceDir.TrimEnd('/'))/$file"
        $sql = "LOAD DATA INFILE '$serverPath' INTO TABLE tpch.$table " +
            "FIELDS TERMINATED BY '|' OPTIONALLY ENCLOSED BY '`"' " +
            "LINES TERMINATED BY '\n'"
        $timer = [System.Diagnostics.Stopwatch]::StartNew()
        $sql | & $mysqlPath @baseArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Loading tpch.$table failed with exit code $LASTEXITCODE."
        }
        $timer.Stop()

        $actual = "SELECT COUNT(*) FROM tpch.$table" | & $mysqlPath @baseArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Counting tpch.$table failed with exit code $LASTEXITCODE."
        }

        $actualCount = [long]($actual | Select-Object -Last 1)
        $expectedCount = [long]$entry.Value.Expected
        if ($actualCount -ne $expectedCount) {
            throw "tpch.$table expected $expectedCount rows, got $actualCount."
        }

        Write-Host ("PASS tpch.{0} rows={1} elapsed={2:n3}s" -f `
            $table, $actualCount, $timer.Elapsed.TotalSeconds)
    }
} finally {
    $env:MYSQL_PWD = $previousPassword
}

Write-Host 'TPC-H SF1 schema, load, and row-count validation passed.'
