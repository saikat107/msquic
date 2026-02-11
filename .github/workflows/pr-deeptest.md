---
description: DeepTest end to end workflow
on:
  pull_request:
    types: [opened, synchronize]
    branches: [master]
permissions:
  contents: read
  pull-requests: read
  issues: read
roles: all
env:
  PR_NUMBER: ${{ inputs.pr_number || github.event.pull_request.number }}
  PR_REPO: ${{ inputs.repo || github.repository }}
  FILTER: ${{ inputs.filter || '^src/.*' }}
  GH_AW_DIR: /tmp/gh-aw
  PR_FILES_PATH: /tmp/gh-aw/pr-files.json
  COVERAGE_RESULT_PATH: /tmp/gh-aw/coverage-result.md
engine:
  id: copilot
  agent: DeepTest
safe-outputs:
  create-pull-request:
    title-prefix: "[Deep Test]"
    labels: [deeptest]
    draft: true
    expires: 7d
  noop:

jobs:
  # workflow call cannot use env.* as input
  # and thus create a step to pass these
  # values directly to list-pr-files
  resolve-params-for-list-pr-files:
    runs-on: ubuntu-slim
    permissions:
      contents: read
    outputs:
      pr_number: ${{ env.PR_NUMBER }}
      pr_repo: ${{ env.PR_REPO }}
      filter: ${{ env.FILTER }}
    steps:
      - name: Export params
        run: echo "Resolved pr_number, pr_repo, filter"
  list-files:
    needs: resolve-params-for-list-pr-files
    uses: ./.github/workflows/list-pr-files.yml
    permissions:
      contents: read
      pull-requests: read
    with:
      # needs.resolve-params-for-list-pr-files.outputs.pr_number is always string type
      # need to convert to number type
      pr_number: ${{ fromJSON(needs.resolve-params-for-list-pr-files.outputs.pr_number) }}
      repo: ${{ needs.resolve-params-for-list-pr-files.outputs.pr_repo }}
      filter: ${{ needs.resolve-params-for-list-pr-files.outputs.filter }}
steps:
  - name: Checkout repository
    uses: actions/checkout@8e8c483db84b4bee98b60c0593521ed34d9990e8 # v6
    with:
      fetch-depth: 1
  - name: Download PR Files List
    uses: actions/download-artifact@fa0a91b85d4f404e444e00e005971372dc801d16 # v4.1.8
    with:
      name: pr-files-list
      path: ${{ env.GH_AW_DIR }}
post-steps:
  - name: Upload Coverage Result
    if: always()
    uses: actions/upload-artifact@b4b15b8c7c6ac21ea08fcf65892d2ee8f75cf882 # v4.4.3
    with:
      name: coverage-result
      path: ${{ env.COVERAGE_RESULT_PATH }}
      if-no-files-found: ignore
---

# DeepTest end to end workflow

Analyze files changed in PR #${{ env.PR_NUMBER }} from repository `${{ env.PR_REPO }}` and generate comprehensive tests.

## PR Files to Analyze

The list of changed files is available at `${{ env.PR_FILES_PATH }}`.

Each file entry contains:
- `path`: The file path relative to the repository root
- `status`: One of `added`, `modified`, or `removed`

## Instructions

You must never attempt to run `git push` as it is not supported in this environment.

### Step 1 — Analyze PR files

Read the PR files list from `${{ env.PR_FILES_PATH }}`:
- **`added`**: Analyze the new code and create comprehensive tests
- **`modified`**: Analyze the changes and update/add tests to cover modifications
- **`removed`**: Check if associated tests should be removed or updated

For each file path, map to relevant test harnesses:
- `src/core/*.c` → Test harnesses: `Basic*`, `Core*`, `Connection*`, `Stream*`
- `src/core/cubic.c` → Test harnesses: `Cubic*`, `CongestionControl*`
- `src/core/loss_detection.c` → Test harnesses: `Loss*`, `Recovery*`
- `src/core/stream.c` → Test harnesses: `Stream*`
- `src/core/connection.c` → Test harnesses: `Connection*`
- `src/platform/*.c` → Test harnesses: `Platform*`, `Datapath*`

### Step 2 — Iterative generate → build → test → coverage loop

Repeat the following cycle until the coverage of the changed source files reaches **90 %** or you have completed **5 iterations** (whichever comes first):

#### 2a. Generate or improve tests
Write (or refine) test code targeting the changed source files identified in Step 1.

#### 2b. Prepare the package
```bash
pwsh ./scripts/prepare-machine.ps1 -ForBuild -ForTest
pwsh ./scripts/prepare-package.ps1
```

#### 2c. Build with code-coverage instrumentation
```bash
pwsh ./scripts/build.ps1 -Config Debug -Tls openssl -CodeCoverage -DisablePerf -DisableTools -CI
```
The `-CodeCoverage` flag enables gcov instrumentation (Linux).

#### 2d. Run the relevant tests
```bash
pwsh ./scripts/test.ps1 -Config Debug -Tls openssl -CodeCoverage -Filter "<HARNESS_FILTER>"
```
Replace `<HARNESS_FILTER>` with the test harness filter that matches the changed files (e.g. `Cubic*` for `cubic.c`). Use `:` to combine multiple filters (e.g. `Cubic*:Connection*`).

#### 2e. Collect and evaluate coverage
After the test run, coverage data is in `artifacts/coverage/msquiccoverage.xml` (Cobertura XML).
Parse the file and compute the **line-coverage percentage** for the changed source files only.

- **≥ 90 %** → Exit the loop and proceed to Step 3.
- **< 90 %** → Analyse which lines/branches are uncovered, generate additional tests, and repeat from 2a.
- **Iteration limit reached (5)** → Proceed to Step 3 with the best coverage achieved.

Log each iteration's coverage percentage so progress is visible in the workflow output.

### Step 3 — Report

Write a Markdown coverage summary to `${{ env.COVERAGE_RESULT_PATH }}` containing:
- Per-file line-coverage percentage for each changed source file
- Overall coverage percentage
- Number of iterations taken
- List of test files created or modified

### Step 4 — Create pull request

Prepare a commit with `scripts/create-commit-for-safe-outputs.sh` and use `create_pull_request` with:
- Title: "Tests for PR #${{ env.PR_NUMBER }}"
- Body: workflow run ${{ github.run_id }}

### Step 5 — No changes fallback

If no staged changes exist, use `noop` with message "No test changes generated for PR #${{ env.PR_NUMBER }}."
