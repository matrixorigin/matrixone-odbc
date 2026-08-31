# Continuous integration

Pull requests and pushes to `main` run `.github/workflows/ci.yml`. The workflow
is intentionally split into checks with stable names so they can be selected as
required status checks in a GitHub ruleset.

## Required checks

- `Source hygiene` rejects whitespace errors in changed lines and parses the
  Power BI and WiX XML inputs.
- `Linux build + MatrixOne smoke` builds both ODBC drivers and `MatrixOne.mez`,
  starts MatrixOne v4.1.4, runs the positive and negative ODBC smoke paths, and
  runs the 13-case deep compatibility suite through both the Unicode and ANSI
  drivers. Known server defects are executable XFAILs linked to MatrixOne
  issues. The MatrixOne archive is pinned by release and SHA-256.
- `macOS ARM64 build` verifies that the driver, smoke executable, and Power BI
  connector build on the architecture used for local development.

The Linux and macOS jobs upload driver libraries, the smoke and deep-test
executables, and `MatrixOne.mez` for 14 days. A failed Linux compatibility run
uploads the MatrixOne log for seven days.

## Merge policy

After the workflow has completed successfully once, configure a ruleset for
`main` that requires the three checks above, requires the branch to be current,
and blocks force pushes and deletion. Keep `pull_request` rather than
`pull_request_target`: CI builds contributor code and must not receive write
permissions or repository secrets.

## Windows package gate

`.github/workflows/windows-package.yml` builds the pinned Windows x64 MSI and
portable ZIP, rejects SDK files in runtime packages, and verifies clean install,
repair, upgrade from the previous published MatrixOne ODBC release, downgrade
blocking, DSN preservation, Oracle MySQL ODBC coexistence, dependency failure
recovery, uninstall, and reinstall. The release revision and previous-package
hash must be advanced together for every release candidate.
