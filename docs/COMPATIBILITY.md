# Compatibility snapshot

Tested on 2026-08-12 with:

- MatrixOne `8.0.30-MatrixOne-v` at `9a7c98b8f3aa07fad24b411d54c7a9f6cb3a8731`
  (`main`, WSL2 Ubuntu 24.04, Windows x64 host)
- MatrixOne ODBC based on MySQL Connector/ODBC `9.7.0`
- MySQL command-line client `9.7.1`
- Power BI Desktop `2.152.1279.0` (64-bit)

## Dedicated MatrixOne coverage

`test/mo_odbc_smoke.c` passes with the Unicode driver registered as
`MatrixOne ODBC 9.7 Unicode Driver` and with `PORT` omitted:

- `SQLDriverConnectW` and default port 6001
- `SQLGetInfo` for DBMS and driver versions
- `SQLGetTypeInfo` (62 rows)
- `SQLTables` and `SQLColumns` for a six-column table
- BIGINT, VARCHAR, DECIMAL, DATE, DATETIME, BOOL, and NULL retrieval
- UTF-8 `SQL_C_CHAR` and UTF-16 `SQL_C_WCHAR` retrieval
- `SQLPrepare`, parameter binding, and execution
- Unicode `SQLExecDirectW`
- authentication failure classification as SQLSTATE `28000`, native error 1045

The deeper `mo_odbc_deep` suite passes through both registered driver variants:

```text
Unicode: 13 passed, 5 expected MatrixOne failures, 0 failed
ANSI:    13 passed, 5 expected MatrixOne failures, 0 failed
```

In addition to the smoke paths, it verifies `SQLGetTypeInfo`, tables and views,
Unicode-aware `SQLColumns`, primary and secondary index metadata,
`SQLDescribeCol`, 20 SQL types, prepared Unicode/decimal/date/binary/NULL,
FLOAT/DOUBLE and boolean values, a Power BI DirectQuery-shaped prepared query,
`SQLDescribeParam`, nested filtering, `COALESCE`, aggregation, HAVING,
LIMIT/OFFSET, VARBINARY-to-wide conversion, unquoted Unicode identifiers,
commit/rollback, 64 KiB data-at-execution and chunked retrieval, SQLSTATE
diagnostics, 72 reads over six concurrent connections, timeout, and
cancellation. MatrixOne `BOOL` is now asserted as ODBC `SQL_BIT` in catalog and
result descriptors while ordinary `TINYINT` remains `SQL_TINYINT`. Catalog
tests also verify that MatrixOne physical helper columns are not exposed by
`SQLColumns`. The five XFAILs are linked to
[matrixone#26678](https://github.com/matrixorigin/matrixone/issues/26678),
[matrixone#26715](https://github.com/matrixorigin/matrixone/issues/26715),
[matrixone#26716](https://github.com/matrixorigin/matrixone/issues/26716), and
[matrixone#26769](https://github.com/matrixorigin/matrixone/issues/26769), plus
the current UTF-8 descriptor regression
[matrixone#26967](https://github.com/matrixorigin/matrixone/issues/26967).

## Public-data validation

TPC-H SF1 was loaded from MatrixOne's published test-data URL: 8 tables,
8,661,255 rows in total, including 6,001,215 `lineitem` rows. MatrixOne's 22
TPC-H queries all passed natively. The dedicated ODBC TPC-H suite then passed
8 cases with one issue-linked descriptor XFAIL and no unknown failures through
each of the Unicode and ANSI drivers. It covers row counts, catalog metadata,
Q1, Q3, Q13, a prepared folding shape, a 100,000-row typed fetch, and eight
parallel analytics clients.

Microsoft's Power Query SDK Test Framework was run at DataConnectors commit
`c7b9d81d0d1a62b5f5486f63087c8587e2ca0160` with its 20,266-row modified NYC
Taxi data set. Sanity passed 8 of 9 cases; Standard functional comparison
passed 188 of 203. Strict DirectQuery folding passed at least 115 of 203;
57 of its 88 failures were PQTest `NullReferenceException` results rather than
confirmed connector folding failures. `FoldListCount` and `FoldTableRowCount`
isolated the MatrixOne prepared aggregate correctness bug
[#26994](https://github.com/matrixorigin/matrixone/issues/26994).

Full hashes, classifications, and release gates are in the
[Chinese deep-test report](POWER_BI_TEST_REPORT_ZH_CN.md).

Missing-table diagnostics now pass after MatrixOne
[4b62e3edd6](https://github.com/matrixorigin/matrixone/commit/4b62e3edd6)
fixed [#26684](https://github.com/matrixorigin/matrixone/issues/26684) by
returning native error 1146; the driver maps it to ODBC SQLSTATE `42S02`.

The older invalid descriptor-length cases from
[#26683](https://github.com/matrixorigin/matrixone/issues/26683) remain fixed,
but current `main` reports a `VARCHAR(128)` utf8mb4 wire length of 384 bytes.
Connector/ODBC consequently exposes `ColumnSize=96` instead of 128. This is
tracked separately as [#26967](https://github.com/matrixorigin/matrixone/issues/26967).

Driver compatibility fixes are covered for uppercase information-schema type
names ([#26680](https://github.com/matrixorigin/matrixone/issues/26680)),
prepared floating-point metadata
([#26682](https://github.com/matrixorigin/matrixone/issues/26682)), and missing
character-set rows needed for Unicode catalog types
([#26688](https://github.com/matrixorigin/matrixone/issues/26688)).

## Real Power BI Desktop validation

Power BI Import and DirectQuery both passed against a seven-column, five-row
fixture containing Chinese text, `DECIMAL`, `DATE`, `DATETIME`, booleans, and
nulls. Import displayed all five rows correctly. A DirectQuery table visual
generated a grouped aggregate and returned `A=30`, `B=30`, `Power BI=-7.25`,
`上海=1234.50`, total `1287.25`.

Reopening the DirectQuery PBIX with the latest driver and clicking Refresh
caused MatrixOne to receive new `SHOW KEYS` and grouped `SUM` statements. The
refreshed visual retained the expected total, proving the live Desktop query
path rather than only a cached PBIX result.

The Windows x64 release shape was also built as MSI and ZIP. Administrative MSI
extraction contained `MatrixOne.mez`, both driver variants, `libmysql.dll`,
OpenSSL/Kerberos/SASL runtime DLLs, and authentication plugins. The extracted
Unicode, ANSI, and setup DLLs passed `LoadLibrary` with only their packaged
directory added to DLL search. No customer-side MySQL SDK was used.

## Upstream differential observations

Selected unmodified Connector/ODBC 9.7 test modules were run against the same
server and driver build:

- `my_tran`: commit and rollback pass. The dedicated suite now also verifies
  `READ COMMITTED` and restores `REPEATABLE READ`; the upstream module still
  includes MySQL-specific isolation expectations that are not release gates.
- `my_catalog3`: wildcard and SQL mode catalog cases pass. After the Unicode
  catalog and ODBC index-order fixes, three of four cases pass; the remaining
  assertion expects MySQL to preserve mixed-case index identifiers while
  MatrixOne returns them lower-case.
- `my_blob`: ordinary 5 KiB BLOB, data-at-execution, length-only retrieval, and
  large binary round trips pass. Five cases stop at unsupported `LONG VARCHAR`
  or `LONG VARBINARY` aliases (#26686), and `TINYTEXT` stores more than its
  MySQL-compatible limit (#26687).
- `my_error`: 18 of 20 cases pass, including the ODBC 2 and 3 missing-table
  cases fixed by MatrixOne #26684. The other two depend on MySQL-specific
  account/password-expiration and stored-procedure error behavior.
- `my_basics`, `my_prepare`, `my_param`, and `my_types` confirm core direct and
  prepared execution, parameter arrays for DML, decimal/bigint conversion,
  cancellation, truncation diagnostics, and result fetching. Their residual
  failures are grouped by the linked MatrixOne issues or by MySQL-only stored
  procedure, ENUM/SET, charset-literal, TLS-fixture, and invalid-date behavior.

These upstream totals are diagnostic, not CI gates: the dedicated suite turns
the Power BI-relevant contracts into stable assertions without treating every
MySQL server feature as required MatrixOne functionality.

The bounded full-module run covered 35 upstream executables: 8 modules passed,
22 failed, 4 terminated inside MySQL-specific test assumptions, and 1 skipped;
313 TAP assertions passed and 117 failed. The residual failures are dominated
by MySQL-only stored procedures, cursor updates, TLS fixtures, account syntax,
VECTOR, and exact MySQL metadata expectations, so module totals are not a
MatrixOne compatibility score.

## Upstream Unicode suite observations

The upstream `my_unicode` executable currently reports 10 passing tests, 2
skips, and 8 failures against this local setup. The failures are grouped rather
than treated as eight independent MatrixOne defects:

- Seven use a valid unquoted BMP character in an identifier. The same SQL fails
  through a native UTF-8 MySQL client, while the backtick-quoted control passes;
  this is the MatrixOne tokenizer compatibility issue #26715.
- One requests a VARBINARY value as `SQL_C_WCHAR`; the driver reports an
  unknown character conversion failure because the result metadata omits
  MySQL `BINARY_FLAG` (#26716).

## Not yet validated

- On-premises data gateway refresh
- Interactive MSI install, repair, and uninstall on a clean Windows VM
- Signed custom connector policy
- TLS certificate verification modes and production authentication variants
