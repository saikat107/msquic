# Ubuntu PR Coverage (GCC + gcovr)

Generate or update tests with Copilot CLI DeepTest, then run MsQuic tests on an Ubuntu runner with GCC/gcov coverage instrumentation and generate reports using `gcovr`.

## What this workflow does

- Determines the list of files changed in the PR (for logging/artifacts)
- Runs the Copilot CLI `DeepTest` agent to generate/update tests in the working tree
- Installs `gcovr` and required system packages
- Builds MsQuic with `--coverage` instrumentation flags
- Runs tests to produce `.gcda` files
- Generates:
  - Cobertura XML: `artifacts/coverage/gcovr-coverage.xml`
  - HTML details: `artifacts/coverage/gcovr-coverage.html`
- Uploads coverage outputs as workflow artifacts
- Creates a new PR for generated tests via gh-aw safe outputs

## Notes

- This workflow is best-effort: if your branch changes require CLOG sidecar updates, run `scripts/update-sidecar.ps1` in the PR.
- This workflow creates a new PR only when DeepTest produced changes.
