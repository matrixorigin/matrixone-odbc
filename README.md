# MatrixOne ODBC

[![CI](https://github.com/iamlinjunhong/matrixone-odbc/actions/workflows/ci.yml/badge.svg)](https://github.com/iamlinjunhong/matrixone-odbc/actions/workflows/ci.yml)

MatrixOne ODBC is a thin, experimental fork of MySQL Connector/ODBC 9.7.0 for
MatrixOne and Power BI DirectQuery compatibility work. It intentionally keeps
the upstream MySQL client protocol and most internal library names unchanged so
that failures remain easy to compare with the upstream driver.

This repository currently produces:

- `myodbc9w` and `myodbc9a`: Unicode and ANSI ODBC driver libraries with the
  registered names `MatrixOne ODBC 9.7 Unicode Driver` and
  `MatrixOne ODBC 9.7 ANSI Driver`.
- `MatrixOne.mez`: a Power BI custom connector using `Odbc.DataSource`, with
  DirectQuery enabled and MatrixOne port `6001` as the default.

## Current scope

The first cut deliberately stays small: it changes user-visible branding,
default port, packaging registration names, Power BI data-source identity,
authentication SQLSTATE mapping, and the Unicode transport charset. MySQL wire
protocol, the broader ODBC API implementation, type mapping, and internal
`myodbc*` library names remain close to upstream. See
[MATRIXONE_CHANGES.md](MATRIXONE_CHANGES.md).

## Build on macOS ARM64

Prerequisites:

```sh
brew install cmake mysql-client unixodbc
```

Configure and build:

```sh
cmake -S . -B build-arm64 \
  -DWITH_UNIXODBC=1 \
  -DMYSQL_DIR=/opt/homebrew/opt/mysql-client \
  -DODBC_INCLUDES=/opt/homebrew/opt/unixodbc/include \
  -DODBC_LIB_DIR=/opt/homebrew/opt/unixodbc/lib \
  -DODBCINST_LIB_DIR=/opt/homebrew/opt/unixodbc/lib \
  -DMYSQLCLIENT_STATIC_LINKING=0 \
  -DMYSQL_LINK_FLAGS=-L/opt/homebrew/opt/zstd/lib \
  -DWITH_PBI=1
cmake --build build-arm64 --parallel
```

The Power BI extension is generated as `build-arm64/pbi/MatrixOne.mez`.
Power BI Desktop testing requires Windows; the ODBC protocol, catalog metadata,
and SQL paths can be exercised independently on macOS or Linux.

Build and run the MatrixOne-specific API test against a registered driver:

```sh
cmake --build build-arm64 --target mo_odbc_smoke
export MO_PASSWORD='your-password'
mysql -h 127.0.0.1 -P 6001 -u root -p < test/mo_odbc_smoke.sql
export MO_ODBC_CONNECTION_STRING="DRIVER={MatrixOne ODBC 9.7 Unicode Driver};SERVER=127.0.0.1;DATABASE=mo_odbc_smoke;UID=root;PWD=${MO_PASSWORD};SSLMODE=DISABLED"
./build-arm64/test/mo_odbc_smoke
```

Leaving `PORT` out of that string intentionally verifies the MatrixOne default
of `6001`. Current results are recorded in
[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

## Continuous integration

Every pull request builds the driver and `MatrixOne.mez` on Linux and macOS
ARM64. The Linux job also starts a pinned MatrixOne release and runs the ODBC
smoke test, including the expected authentication failure path. See
[docs/CI.md](docs/CI.md) for check names, artifact retention, and the recommended
merge ruleset.

## Diagnostics

Use the layered workflow in [docs/POWER_BI.md](docs/POWER_BI.md). In short,
first reproduce with the ODBC smoke test, then enable unixODBC or Windows ODBC
tracing, and only then enable Power Query connector tracing. This identifies
whether a failure belongs to connection setup, wire protocol, ODBC metadata,
Power Query translation, or MatrixOne SQL semantics.

## Upstream and license

The baseline is MySQL Connector/ODBC tag `9.7.0`, commit
`70150742aced011228424c39f9322993d9a468d2`. Original Oracle copyrights and
license notices are retained. This derivative remains under GPL-2.0 with the
Universal FOSS Exception; see [LICENSE.txt](LICENSE.txt).

MySQL is a trademark of Oracle and/or its affiliates. MatrixOne ODBC is not an
Oracle product and is not supported by Oracle.
