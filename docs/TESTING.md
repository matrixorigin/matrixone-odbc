# MatrixOne ODBC test strategy

The test plan separates failures by layer before assigning them to the driver,
MatrixOne, the driver manager, or Power BI. A green driver build is not treated
as evidence of database compatibility.

## Automated layers

1. **Build and packaging** builds the ANSI and Unicode drivers, the dedicated
   test executables, and `MatrixOne.mez` on Linux and macOS ARM64.
2. **Connection and diagnostics** covers `SQLDriverConnectW`, MatrixOne's
   default port, capabilities, authentication SQLSTATE, syntax errors, missing
   objects, duplicate keys, cancellation, and timeout behavior.
3. **Catalog and descriptors** covers `SQLGetTypeInfo`, `SQLTables`,
   `SQLColumns`, `SQLPrimaryKeys`, `SQLStatistics`, wildcard escaping,
   ANSI/Unicode type classification, result types, precision, and declared
   length.
4. **Data and execution** covers scalar types, Unicode text, binary and JSON,
   NULL, prepared parameters, floating-point precision, date structures,
   data-at-execution, chunked `SQLGetData`, transactions, and isolation.
5. **Stress** opens six simultaneous connections and performs 72 catalog-backed
   reads. This is a bounded concurrency smoke test, not a throughput benchmark.
6. **Upstream differential tests** run selected MySQL Connector/ODBC modules
   against the same server. A failure is reduced to native SQL or protocol
   metadata before it is classified; MySQL-only stored procedure, account,
   collation, and server-configuration expectations are not automatically
   MatrixOne bugs.

`test/mo_odbc_deep.cc` is a public-ODBC-API TAP suite and creates then removes
its own `mo_odbc_deep` database. Run it once for each registered driver:

```sh
export MO_ODBC_CONNECTION_STRING='DRIVER={MatrixOne ODBC 9.7 Unicode Driver};SERVER=127.0.0.1;UID=root;PWD=111;SSLMODE=DISABLED'
./build-arm64/test/mo_odbc_deep

export MO_ODBC_CONNECTION_STRING='DRIVER={MatrixOne ODBC 9.7 ANSI Driver};SERVER=127.0.0.1;UID=root;PWD=111;SSLMODE=DISABLED'
./build-arm64/test/mo_odbc_deep
```

The 13 TAP cases cover:

| Case | Main contract |
| --- | --- |
| Connection capabilities | DBMS/driver identity, required APIs, transactions, `SQLGetTypeInfo` |
| Catalog metadata | tables, views, columns, primary keys, indexes, wildcard behavior |
| Result descriptors | ODBC type, precision, and declared column size |
| Type round trip | integers, decimal, bool, temporal, Unicode, binary, LOB, JSON, NULL |
| Prepared parameters | wide text, decimal, date, binary, NULL, result verification |
| Prepared floating point | FLOAT/DOUBLE values retain fractional precision |
| Transactions | rollback and commit visibility |
| Transaction isolation | requested isolation is actually applied by MatrixOne |
| Streaming | 64 KiB data-at-execution plus chunked text/binary retrieval |
| SQLSTATE classes | syntax, duplicate key, and exact missing-table classification |
| Concurrent connections | six clients, twelve reads each |
| Query timeout | one-second timeout interrupts a two-second query |
| Statement cancellation | `SQLCancel` interrupts `sleep(5)` promptly |

## Known failures and ownership

Expected failures are executable assertions. They turn into normal passes when
the server is fixed, and any unrelated behavior still fails the suite.

| Issue | Layer | Automated status |
| --- | --- | --- |
| [matrixone#26648](https://github.com/matrixorigin/matrixone/issues/26648) | transaction isolation is accepted but ignored | XFAIL |
| [matrixone#26678](https://github.com/matrixorigin/matrixone/issues/26678) | `max_execution_time` is not enforced | XFAIL |
| [matrixone#26680](https://github.com/matrixorigin/matrixone/issues/26680) | uppercase `DATA_TYPE` breaks case-sensitive `SQLColumns` mapping | driver compatibility fix, passing |
| [matrixone#26682](https://github.com/matrixorigin/matrixone/issues/26682) | prepared DOUBLE metadata reports zero decimals | MatrixOne-specific native bind workaround, passing |
| [matrixone#26683](https://github.com/matrixorigin/matrixone/issues/26683) | result metadata reports invalid VARCHAR/VARBINARY lengths | XFAIL |
| [matrixone#26684](https://github.com/matrixorigin/matrixone/issues/26684) | missing table uses native error 1064 instead of 1146 | XFAIL |
| [matrixone#26686](https://github.com/matrixorigin/matrixone/issues/26686) | `LONG VARCHAR` / `LONG VARBINARY` aliases are rejected | upstream differential |
| [matrixone#26687](https://github.com/matrixorigin/matrixone/issues/26687) | `TINYTEXT` does not enforce the 255-byte limit | upstream differential |
| [matrixone#26688](https://github.com/matrixorigin/matrixone/issues/26688) | character-set information schema is empty/inconsistent | driver Unicode fallback, passing |

The umbrella request for first-class support and shared acceptance criteria is
[matrixone#26677](https://github.com/matrixorigin/matrixone/issues/26677).

## Failure attribution checklist

Every new report should record the MatrixOne version/commit, driver build,
driver-manager version, ANSI or Unicode path, exact SQLSTATE and native error,
and a minimal SQL/ODBC reproducer. Then reduce in this order:

1. Run the SQL through a native MySQL protocol client.
2. Inspect native error codes or `--column-type-info -vvv` field metadata.
3. Reproduce through a public ODBC API without Power BI.
4. Compare with the unmodified upstream Connector/ODBC test where applicable.
5. Only then reproduce in the Power BI connector and capture ODBC and Power
   Query traces.

This ordering makes issue ownership explicit and keeps driver workarounds small
and removable.

## Windows and Power BI acceptance

The automated jobs do not replace these release gates:

- MSI side-by-side install, upgrade, and uninstall with Oracle MySQL ODBC
- Unicode and ANSI DSN creation in the 64-bit ODBC Administrator
- Power BI Desktop Import and DirectQuery schema discovery
- numeric, temporal, Unicode, binary, and NULL model-type verification
- folding of filters, projections, grouping, ordering, limits, and joins
- cancellation and timeout from Power BI Desktop
- scheduled refresh through an on-premises data gateway
- TLS CA/hostname verification and bad-certificate diagnostics

Record the generated SQL and SQLSTATE for every failure. A Power BI-only
failure should not be filed against MatrixOne until the native ODBC path passes.
