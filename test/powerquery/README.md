# Power Query SDK compatibility tests

These files adapt Microsoft's Power Query SDK Test Framework to MatrixOne.
The framework exercises 212 sanity and standard cases covering navigation,
schema discovery, query folding, data types, arithmetic, dates and times, text,
filtering, grouping, sorting, limits, and joins.

## Public dataset

Clone `https://github.com/microsoft/DataConnectors` and use the CSV files in
`testframework/data`. The test data is a modified sample of the NYC Taxi and
Limousine Commission green-trip and zone-lookup data, published by Microsoft
under the CDLA-Permissive-2.0 license. The hashes in `load_powerquery.ps1`
pin the files from DataConnectors commit
`c7b9d81d0d1a62b5f5486f63087c8587e2ca0160`.

Load the data with a MySQL command-line client. This client is test tooling
only; it is not a runtime dependency of MatrixOne ODBC or the Power BI
connector.

```powershell
.\load_powerquery.ps1 `
  -MySqlExe C:\path\to\mysql.exe `
  -PluginDir C:\path\to\lib\plugin `
  -DataDir C:\path\to\DataConnectors\testframework\data
```

Build `MatrixOne.mez` and set its `UsernamePassword` credential with
`PQTest.exe`. Then use the wrapper below; it verifies the pinned framework
commit and copies the MatrixOne settings into the official framework layout,
so the test does not depend on where either repository was cloned:

```powershell
.\run_powerquery_tests.ps1 `
  -DataConnectorsDir C:\path\to\DataConnectors `
  -PQTestExe C:\path\to\PQTest.exe `
  -ExtensionPath C:\path\to\MatrixOne.mez `
  -ValidateQueryFolding
```

Use `-FailOnFoldingFailure` for the stricter DirectQuery run. It treats every
query that cannot be completely folded as a failure and writes diagnostic
traces under the framework's `Diagnostics\MatrixOne` directory. The functional
run and the strict folding run should be reported separately because an
expression can return the correct value after local Power Query evaluation
without satisfying DirectQuery folding requirements.
