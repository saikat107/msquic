# Windows PR Coverage (OpenCppCoverage)

Generate or update tests with Copilot CLI DeepTest, then run MsQuic tests on a Windows runner with code coverage enabled via OpenCppCoverage.

## What this workflow does

- Determines the list of files changed in the PR (for logging/artifacts)
- Runs the Copilot CLI `DeepTest` agent to generate/update tests in the working tree
- Installs OpenCppCoverage via `scripts/prepare-machine.ps1 -InstallCodeCoverage`
- Builds and runs the configured test subset with `scripts/test.ps1 -CodeCoverage`
- Uploads coverage outputs from `artifacts/coverage/`
- Creates a new PR for generated tests via gh-aw safe outputs

## Notes

- Coverage generation on Windows uses OpenCppCoverage and produces Cobertura-style XML.
- This workflow creates a new PR only when DeepTest produced changes.
