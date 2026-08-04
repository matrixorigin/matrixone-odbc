# MatrixOne thin-fork changes

Baseline: MySQL Connector/ODBC 9.7.0
(`70150742aced011228424c39f9322993d9a468d2`).

## Initial changes

- Register the public driver names as `MatrixOne ODBC 9.7 Unicode Driver` and
  `MatrixOne ODBC 9.7 ANSI Driver`.
- Use MatrixOne's MySQL-compatible listener port `6001` when `PORT` is omitted.
- Rename the bundled Power BI connector to `MatrixOne.mez`, its data-source
  kind to `MatrixOne`, and its entry point to `MatrixOne.Contents`.
- Keep DirectQuery, `SQLGetInfo`, `SQLColumns`, `SQLGetTypeInfo`, and diagnostic
  hooks from upstream so compatibility failures can be localized.
- Map MatrixOne/MySQL native authentication errors 1044 and 1045 to the ODBC
  authorization SQLSTATE `28000` instead of the upstream fallback `HY000`.
- Force Unicode-driver binaries to select `utf8mb4` in libmysqlclient before
  the handshake, even when a driver manager enters through an ANSI connection
  function, so wide SQL and MatrixOne's UTF-8 parser use the same encoding.
- Update installer, archive, and package registration names while retaining
  the internal `myodbc*` libraries for a small and comparable code delta.
- Give the Windows MSI separate upgrade/component identities and a separate
  `Matrix Origin\\MatrixOne ODBC` install directory so it cannot upgrade or
  uninstall an Oracle MySQL ODBC installation.
- Retain the upstream MySQL Power BI icons as private-preview placeholders;
  replace them with approved MatrixOne artwork before public distribution.

## Deliberate non-changes

- No MatrixOne-specific protocol fork.
- No speculative SQL rewriting or type mapping changes before a failing test
  demonstrates the need.
- No JDBC bridge. Power BI's connector path is ODBC-native.
- No upstream copyright or license removal.

Each future compatibility fix should include a minimal ODBC or SQL reproducer,
the observed MatrixOne version, SQLSTATE/native error, and the layer where the
fix is applied.
