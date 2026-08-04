# Compatibility snapshot

Tested on 2026-08-04 with:

- MatrixOne `8.0.30-MatrixOne-v25908` (`mo-service`, Darwin ARM64)
- MatrixOne ODBC based on MySQL Connector/ODBC `9.7.0`
- Homebrew MySQL client `9.7.1` and unixODBC `2.3.14`
- macOS ARM64

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
Unicode: 9 passed, 4 expected MatrixOne failures, 0 failed
ANSI:    9 passed, 4 expected MatrixOne failures, 0 failed
```

In addition to the smoke paths, it verifies `SQLGetTypeInfo`, tables and views,
Unicode-aware `SQLColumns`, primary and secondary index metadata,
`SQLDescribeCol`, 20 SQL types, prepared Unicode/decimal/date/binary/NULL and
FLOAT/DOUBLE values, commit/rollback, 64 KiB data-at-execution and chunked
retrieval, SQLSTATE diagnostics, 72 reads over six concurrent connections,
timeout, and cancellation. The four XFAILs are linked to
[matrixone#26648](https://github.com/matrixorigin/matrixone/issues/26648),
[matrixone#26678](https://github.com/matrixorigin/matrixone/issues/26678),
[matrixone#26683](https://github.com/matrixorigin/matrixone/issues/26683), and
[matrixone#26684](https://github.com/matrixorigin/matrixone/issues/26684).

Driver compatibility fixes are covered for uppercase information-schema type
names ([#26680](https://github.com/matrixorigin/matrixone/issues/26680)),
prepared floating-point metadata
([#26682](https://github.com/matrixorigin/matrixone/issues/26682)), and missing
character-set rows needed for Unicode catalog types
([#26688](https://github.com/matrixorigin/matrixone/issues/26688)).

## Upstream differential observations

Selected unmodified Connector/ODBC 9.7 test modules were run against the same
server and driver build:

- `my_tran`: commit and rollback pass; the only failing case is the known
  isolation no-op in #26648.
- `my_catalog3`: wildcard and SQL mode catalog cases pass. After the Unicode
  catalog and ODBC index-order fixes, three of four cases pass; the remaining
  assertion expects MySQL to preserve mixed-case index identifiers while
  MatrixOne returns them lower-case.
- `my_blob`: ordinary 5 KiB BLOB, data-at-execution, length-only retrieval, and
  large binary round trips pass. Five cases stop at unsupported `LONG VARCHAR`
  or `LONG VARBINARY` aliases (#26686), and `TINYTEXT` stores more than its
  MySQL-compatible limit (#26687).
- `my_error`: 16 of 20 cases pass. ODBC 2 and 3 missing-table classification
  account for two failures (#26684); the other two depend on MySQL-specific
  account/password-expiration and stored-procedure error behavior.
- `my_basics`, `my_prepare`, `my_param`, and `my_types` confirm core direct and
  prepared execution, parameter arrays for DML, decimal/bigint conversion,
  cancellation, truncation diagnostics, and result fetching. Their residual
  failures are grouped by the linked MatrixOne issues or by MySQL-only stored
  procedure, ENUM/SET, charset-literal, TLS-fixture, and invalid-date behavior.

These upstream totals are diagnostic, not CI gates: the dedicated suite turns
the Power BI-relevant contracts into stable assertions without treating every
MySQL server feature as required MatrixOne functionality.

## Upstream Unicode suite observations

The upstream `my_unicode` executable currently reports 9 passing tests, 2
skips, and 9 failures against this local setup. The failures are grouped rather
than treated as nine independent MatrixOne defects:

- Seven use `SQLConnectW` with a DSN and then a non-ASCII identifier. On this
  macOS/unixODBC path the command reaches MatrixOne with a replacement
  character, while the Power BI-like `SQLDriverConnectW` path in the dedicated
  smoke test sends Unicode SQL correctly. This needs a Windows driver-manager
  run before deciding whether a driver fix is required.
- One constructs the upstream short driver name
  `MatrixOne ODBC 9.7 Driver`, which is intentionally not the registered public
  name `MatrixOne ODBC 9.7 Unicode Driver`.
- One requests a VARBINARY value as `SQL_C_WCHAR`; the driver reports an
  unknown character conversion failure. This is not used by the current Power
  BI metadata hook but remains a compatibility case to decide explicitly.

## Not yet validated

- Power BI Desktop Import and DirectQuery on Windows
- On-premises data gateway refresh
- Windows MSI build/install/uninstall and signed custom connector policy
- TLS certificate verification modes and production authentication variants
- DirectQuery SQL generated by real semantic models
