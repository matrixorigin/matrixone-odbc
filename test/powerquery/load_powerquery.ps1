[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MySqlExe,

    [Parameter(Mandatory = $true)]
    [string]$PluginDir,

    [Parameter(Mandatory = $true)]
    [string]$DataDir,

    [string]$Server = 'localhost',
    [int]$Port = 6001,
    [string]$User = 'root',
    [string]$Password = '111'
)

$ErrorActionPreference = 'Stop'

$mysqlPath = (Resolve-Path $MySqlExe).Path
$pluginPath = (Resolve-Path $PluginDir).Path
$dataPath = (Resolve-Path $DataDir).Path
$schemaPath = Join-Path $PSScriptRoot 'schema.sql'

$baseArgs = @(
    '--protocol=TCP'
    "--host=$Server"
    "--port=$Port"
    "--user=$User"
    '--ssl-mode=DISABLED'
    "--plugin-dir=$pluginPath"
    '--local-infile=1'
    '--default-character-set=utf8mb4'
    '--batch'
    '--skip-column-names'
)

$tables = [ordered]@{
    NycTaxiData = @{
        File = 'nyc_taxi_tripdata.csv'
        Expected = 10000
        Sha256 = 'CA5389B809077C0CC14B8CE9CCAE5EAC83CB8966F3106F5FA3E95B490DB0B2B8'
    }
    NycTaxiDateData = @{
        File = 'nyc_taxi_trip_date_data.csv'
        Expected = 10000
        Sha256 = 'B220C34567F3236D60DA07E58FCB24EF221C983B9B72B232288D15E99E4EA4DF'
    }
    TaxiZoneLookup = @{
        File = 'taxi+_zone_lookup.csv'
        Expected = 265
        Sha256 = '598BFDD5170DCD8B9CFB5EC7F8DC283552E1B79CF0BCF3B7A0463FEC1BFEF115'
    }
    misc_table = @{
        File = 'misc_table.csv'
        Expected = 1
        Sha256 = '05590B86198B12BE1B4B9E29330522AA12AE44479B2B16745E12D8E8293BBF90'
    }
}

$previousPassword = $env:MYSQL_PWD
$env:MYSQL_PWD = $Password

try {
    $schemaProcess = Start-Process -FilePath $mysqlPath -ArgumentList $baseArgs `
        -RedirectStandardInput $schemaPath -NoNewWindow -Wait -PassThru
    if ($schemaProcess.ExitCode -ne 0) {
        throw "Power Query schema creation failed with exit code $($schemaProcess.ExitCode)."
    }

    foreach ($entry in $tables.GetEnumerator()) {
        $table = $entry.Key
        $filePath = Join-Path $dataPath $entry.Value.File
        if (!(Test-Path -LiteralPath $filePath)) {
            throw "Missing Power Query SDK dataset file: $filePath"
        }

        $actualHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash
        if ($actualHash -ne $entry.Value.Sha256) {
            throw "SHA256 mismatch for $filePath. Expected $($entry.Value.Sha256), got $actualHash."
        }

        $sqlPath = (Resolve-Path -LiteralPath $filePath).Path.Replace('\', '/')
        $sql = "LOAD DATA LOCAL INFILE '$sqlPath' INTO TABLE pqsdk_test.$table " +
            "FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '`"' " +
            "LINES TERMINATED BY '\r\n' IGNORE 1 LINES"

        $timer = [System.Diagnostics.Stopwatch]::StartNew()
        $sql | & $mysqlPath @baseArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Loading pqsdk_test.$table failed with exit code $LASTEXITCODE."
        }
        $timer.Stop()

        $actual = "SELECT COUNT(*) FROM pqsdk_test.$table" | & $mysqlPath @baseArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Counting pqsdk_test.$table failed with exit code $LASTEXITCODE."
        }

        $actualCount = [long]($actual | Select-Object -Last 1)
        $expectedCount = [long]$entry.Value.Expected
        if ($actualCount -ne $expectedCount) {
            throw "pqsdk_test.$table expected $expectedCount rows, got $actualCount."
        }

        Write-Host ("PASS pqsdk_test.{0} rows={1} elapsed={2:n3}s" -f `
            $table, $actualCount, $timer.Elapsed.TotalSeconds)
    }
} finally {
    $env:MYSQL_PWD = $previousPassword
}

Write-Host 'Power Query SDK public dataset load and row-count validation passed.'
