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
traces into a fresh temporary directory for each invocation. The functional
run and the strict folding run should be reported separately because an
expression can return the correct value after local Power Query evaluation
without satisfying DirectQuery folding requirements.

### Boolean slicer regression

After loading the same public fixture, run `BooleanSlicer.query.pq` with
`PQTest.exe run-test -p -e <MatrixOne.mez> -q <query> --failOnFoldingFailure`.
It must fold boolean-to-Int64 conversion, distinct, sorting, and LIMIT 501,
returning null/0/1 for the pinned fixture. This catches the conversion needed
by Power BI DirectQuery boolean slicers. Also rerun the standard DecimalFrom
and Precision.Decimal tests: globally advertising CAST fixes the slicer but
can silently truncate fractions through MO's unqualified DECIMAL type.

Desktop acceptance remains separate: restart Power BI after replacing the
MEZ, use a DirectQuery table containing TRUE/FALSE/NULL, and verify each
slicer selection filters a dependent aggregate correctly. SDK folding alone
is not evidence that a Desktop visual works.
