# Power BI and compatibility diagnostics

## Install shape

Power BI Desktop and an on-premises data gateway must both have:

1. The matching 64-bit `MatrixOne ODBC 9.7 Unicode Driver` installed.
2. `MatrixOne.mez` copied to the Power BI custom connectors directory.
3. The same connector and driver versions when scheduled refresh uses a
   gateway.

The connector expects `server[:port][;database]`; port `6001` is used when the
port is omitted. It uses username/password authentication and requests
opportunistic TLS (`SSLMODE=PREFERRED`). This works with MatrixOne's default
unencrypted local deployment while still preferring TLS when the server offers
it.

The Windows x64 MSI contains the driver, client runtime DLLs, authentication
plugins, and `MatrixOne.mez`. End users do not need the MySQL SDK or a MySQL
Server installation. The installer checks for the Visual C++ 2022 x64 runtime.

The preview connector is unsigned. In Power BI Desktop, enable loading of
unvalidated custom extensions under **Options and settings > Options >
Security > Data Extensions**, restart Power BI, and then select **MatrixOne
Database with DirectQuery Support** from **Get data**. Production distribution
should sign the connector and restore the validated-extension policy.

## Failure isolation

Reproduce in this order:

1. **Native client:** connect with the MySQL command-line client. Failure here
   is below ODBC, commonly networking, authentication, TLS, or wire protocol.
2. **ODBC connection:** connect using `isql`/`iusql` or an ODBC API smoke test.
   Record SQLSTATE, native error, driver version, and server version.
3. **ODBC catalog APIs:** call `SQLGetInfo`, `SQLGetTypeInfo`, `SQLTables`, and
   `SQLColumns`. Navigator failures usually appear here.
4. **Power BI Import mode:** this separates connector discovery and type
   conversion from DirectQuery SQL generation.
5. **Power BI DirectQuery:** capture the generated SQL and replay it directly
   against MatrixOne. If replay fails, reduce it to a MatrixOne SQL case; if it
   succeeds, inspect ODBC result metadata and parameter binding.

## Tracing

- Enable Windows ODBC tracing from **ODBC Data Sources (64-bit) > Tracing** only
  for a short reproduction because it records connection activity and may
  include sensitive values.
- For Power Query details, make a development build with
  `EnableTraceOutput = true` in `pbi/MatrixOne.pq`, reproduce once, then restore
  it to `false`. The connector logs `SQLGetInfo`, `SQLGetTypeInfo`,
  `SQLColumns`, and error-hook activity.
- Capture MatrixOne server logs using the same time window and connection user.
- Only native authentication errors 1044 and 1045 invalidate stored Power BI
  credentials. Syntax, metadata, and query errors are returned unchanged so
  they do not trigger a misleading password prompt.

Do not commit passwords, complete production connection strings, or raw traces
without redaction.
