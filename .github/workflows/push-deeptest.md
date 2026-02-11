---
description: DeepTest end to end workflow (push to master)
on:
  push:
    branches: [master]
    paths:
      - 'src/**'
permissions:
  contents: read
  pull-requests: read
  issues: read
roles: all
env:
  COMMIT_SHA: ${{ github.sha }}
  BEFORE_SHA: ${{ github.event.before }}
  REPO: ${{ github.repository }}
  FILTER: '^src/.*'
  GH_AW_DIR: /tmp/gh-aw
  CHANGED_FILES_PATH: /tmp/gh-aw/pr-files.json
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

steps:
  - name: Checkout repository
    uses: actions/checkout@8e8c483db84b4bee98b60c0593521ed34d9990e8 # v6
    with:
      fetch-depth: 0
  - name: Collect changed files from push
    env:
      GH_TOKEN: ${{ github.token }}
    shell: bash
    run: |
      set -euo pipefail
      BEFORE="${{ env.BEFORE_SHA }}"
      AFTER="${{ env.COMMIT_SHA }}"
      REPO="${{ env.REPO }}"
      FILTER="${{ env.FILTER }}"
      API_URL="${GITHUB_API_URL:-https://api.github.com}"
      OUT="${{ env.CHANGED_FILES_PATH }}"
      mkdir -p "$(dirname "$OUT")"

      # Handle new-branch push where before is all zeros
      if [ "$BEFORE" = "0000000000000000000000000000000000000000" ]; then
        echo "Initial branch push, comparing against first parent"
        BEFORE="$(git rev-parse HEAD~1 2>/dev/null || echo "$AFTER")"
      fi

      # Use GitHub Compare API to get changed files
      resp="$(curl -fsSL \
        -H "Authorization: Bearer $GH_TOKEN" \
        -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" \
        "$API_URL/repos/$REPO/compare/${BEFORE}...${AFTER}")"

      # Extract files array, apply filter, build JSON
      printf '%s' "$resp" | jq --arg filter "$FILTER" '
        .files // []
        | [.[] | {path: .filename, status: .status}]
        | if $filter != "" then [.[] | select(.path | test($filter))] else . end
      ' > "$OUT"

      echo "Collected $(jq length "$OUT") changed files (compare ${BEFORE:0:7}...${AFTER:0:7}):"
      jq -r '.[] | "  \(.status)  \(.path)"' "$OUT"

      # Try to find associated merged PR number for reference
      pr_resp="$(curl -fsSL \
        -H "Authorization: Bearer $GH_TOKEN" \
        -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" \
        "$API_URL/repos/$REPO/commits/$AFTER/pulls" 2>/dev/null || echo '[]')"
      PR_NUM="$(printf '%s' "$pr_resp" | jq -r '.[0].number // empty' 2>/dev/null || true)"
      if [ -n "$PR_NUM" ]; then
        echo "Associated PR: #$PR_NUM"
        echo "MERGED_PR_NUMBER=$PR_NUM" >> "$GITHUB_ENV"
      else
        echo "No associated PR found for commit $AFTER"
      fi

post-steps:
  - name: Upload Coverage Result
    if: always()
    uses: actions/upload-artifact@b4b15b8c7c6ac21ea08fcf65892d2ee8f75cf882 # v4.4.3
    with:
      name: coverage-result
      path: ${{ env.COVERAGE_RESULT_PATH }}
      if-no-files-found: ignore
---

# DeepTest end to end workflow (push to master)

Analyze files changed in push commit `${{ env.COMMIT_SHA }}` to the master branch of repository `${{ env.REPO }}` and generate comprehensive tests.

## Changed Files to Analyze

The list of changed files is available at `${{ env.CHANGED_FILES_PATH }}`.

Each file entry contains:
- `path`: The file path relative to the repository root
- `status`: One of `added`, `modified`, or `removed`

## Instructions

You must never attempt to run `git push` as it is not supported in this environment.

### Step 1 — Analyze changed files

Read the changed files list from `${{ env.CHANGED_FILES_PATH }}`:
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
- Title: "Tests for push ${{ env.COMMIT_SHA }}"
- Body: workflow run ${{ github.run_id }}

### Step 5 — No changes fallback

If no staged changes exist, use `noop` with message "No test changes generated for push ${{ env.COMMIT_SHA }}."
