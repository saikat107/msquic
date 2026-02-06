# Windows PR Coverage (OpenCppCoverage)

Run MsQuic tests on a Windows runner with code coverage enabled via OpenCppCoverage.

## What this workflow does

- Determines the list of files changed in the PR (for logging/artifacts)
- Installs OpenCppCoverage via `scripts/prepare-machine.ps1 -InstallCodeCoverage`
- Builds and runs the configured test subset with `scripts/test.ps1 -CodeCoverage`
- Uploads coverage outputs from `artifacts/coverage/`

## Notes

- Coverage generation on Windows uses OpenCppCoverage and produces Cobertura-style XML.
- This workflow intentionally does not post PR comments; it only uploads artifacts.
