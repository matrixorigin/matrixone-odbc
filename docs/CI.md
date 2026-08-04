# Continuous integration

Pull requests and pushes to `main` run `.github/workflows/ci.yml`. The workflow
is intentionally split into checks with stable names so they can be selected as
required status checks in a GitHub ruleset.

## Required checks

- `Source hygiene` rejects whitespace errors in changed lines and parses the
  Power BI and WiX XML inputs.
- `Linux build + MatrixOne smoke` builds both ODBC drivers and `MatrixOne.mez`,
  starts MatrixOne v4.1.4, and runs the dedicated positive and negative ODBC
  smoke paths. The MatrixOne archive is pinned by release and SHA-256.
- `macOS ARM64 build` verifies that the driver, smoke executable, and Power BI
  connector build on the architecture used for local development.

The Linux and macOS jobs upload driver libraries, the smoke executable, and
`MatrixOne.mez` for 14 days. A failed Linux smoke test uploads the MatrixOne log
for seven days.

## Merge policy

After the workflow has completed successfully once, configure a ruleset for
`main` that requires the three checks above, requires the branch to be current,
and blocks force pushes and deletion. Keep `pull_request` rather than
`pull_request_target`: CI builds contributor code and must not receive write
permissions or repository secrets.

## Deliberate next step

This first gate does not claim Windows coverage. Add a Windows driver/MSI job
after the MySQL client SDK and WiX toolchain are pinned and the resulting MSI
has been tested for side-by-side install and uninstall with Oracle MySQL ODBC.
