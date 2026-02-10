---
description: Generate tests for PR-changed files targeting 95% coverage (Ubuntu + gcovr)
on:
  pull_request:
    branches: [master]
    types: [opened, synchronize, reopened, ready_for_review]
  workflow_dispatch:
    inputs:
      pr_number:
        description: "Pull request number (required when manually dispatching)"
        required: true
        type: string
      agent_name:
        description: "Copilot custom agent name"
        required: false
        default: "DeepTest"
        type: string
      max_iterations:
        description: "Max agent iterations to try before stopping"
        required: false
        default: "5"
        type: string
      coverage_target:
        description: "Target line coverage percentage for the changed C/C++ files"
        required: false
        default: "95"
        type: string
      config:
        description: "Build/test configuration (Debug recommended for coverage)"
        required: false
        default: "Debug"
        type: string
      arch:
        description: "CPU architecture"
        required: false
        default: "x64"
        type: string
      tls:
        description: "TLS provider"
        required: false
        default: "openssl"
        type: string
      max_files:
        description: "Max number of changed files to include"
        required: false
        default: "10"
        type: string
      include_regex:
        description: "grep -E include filter (applied to path list)"
        required: false
        default: "^src/"
        type: string
      exclude_regex:
        description: "grep -E exclude filter (applied to path list)"
        required: false
        default: "(\\.md$|\\.ya?ml$|\\.json$|\\.txt$|^docs/|^submodules/|^src/test/)"
        type: string

permissions:
  contents: read
  issues: read
  pull-requests: read

strict: false

runs-on: ubuntu-latest

env:
  GH_TOKEN: ${{ github.token }}
  COPILOT_GITHUB_TOKEN: ${{ secrets.COPILOT_GITHUB_TOKEN }}
  RUN_ID: ${{ github.run_id }}
  PR_NUMBER: ${{ github.event.pull_request.number || github.event.inputs.pr_number }}
  AGENT_NAME: ${{ github.event.inputs.agent_name || 'DeepTest' }}
  MAX_ITERATIONS: ${{ github.event.inputs.max_iterations || '5' }}
  COVERAGE_TARGET: ${{ github.event.inputs.coverage_target || '95' }}
  CONFIG: ${{ github.event.inputs.config || 'Debug' }}
  ARCH: ${{ github.event.inputs.arch || 'x64' }}
  TLS: ${{ github.event.inputs.tls || 'openssl' }}
  MAX_FILES: ${{ github.event.inputs.max_files || '10' }}
  INCLUDE_REGEX: ${{ github.event.inputs.include_regex || '^src/' }}
  EXCLUDE_REGEX: ${{ github.event.inputs.exclude_regex || '(\.md$|\.ya?ml$|\.json$|\.txt$|^docs/|^submodules/|^src/test/)' }}
  COVERAGE_XML: artifacts/coverage/gcovr-coverage.xml

engine:
  id: custom
  steps:
    - name: Fetch PR metadata
      shell: bash
      run: |
        set -euo pipefail
        if [ -z "${PR_NUMBER:-}" ]; then
          echo "PR_NUMBER is required" >&2
          exit 1
        fi

        API_URL="${GITHUB_API_URL:-https://api.github.com}"
        pr_json="$(curl -fsSL \
          -H "Authorization: Bearer $GH_TOKEN" \
          -H "Accept: application/vnd.github+json" \
          -H "X-GitHub-Api-Version: 2022-11-28" \
          "$API_URL/repos/$GITHUB_REPOSITORY/pulls/$PR_NUMBER")"

        head_ref="$(printf '%s' "$pr_json" | jq -r '.head.ref')"
        base_ref="$(printf '%s' "$pr_json" | jq -r '.base.ref')"
        head_sha="$(printf '%s' "$pr_json" | jq -r '.head.sha')"
        base_sha="$(printf '%s' "$pr_json" | jq -r '.base.sha')"
        if [ -z "$head_ref" ] || [ "$head_ref" = "null" ] || [ -z "$base_ref" ] || [ "$base_ref" = "null" ] || \
           [ -z "$head_sha" ] || [ "$head_sha" = "null" ] || [ -z "$base_sha" ] || [ "$base_sha" = "null" ]; then
          echo "Unable to determine PR head/base refs for PR #$PR_NUMBER" >&2
          exit 1
        fi

        echo "PR_HEAD_REF=$head_ref" >> "$GITHUB_ENV"
        echo "PR_BASE_REF=$base_ref" >> "$GITHUB_ENV"
        echo "PR_HEAD_SHA=$head_sha" >> "$GITHUB_ENV"
        echo "PR_BASE_SHA=$base_sha" >> "$GITHUB_ENV"
        echo "PR head ref: $head_ref"
        echo "PR base ref: $base_ref"
        echo "PR head sha: $head_sha"
        echo "PR base sha: $base_sha"

    - name: Install Copilot CLI
      shell: bash
      run: |
        set -euo pipefail
        gh extension install github/gh-copilot || echo "Copilot CLI already installed"

    - name: Collect changed files from PR (git diff)
      shell: bash
      run: |
        set -euo pipefail
        if [ -z "${PR_NUMBER:-}" ]; then
          echo "PR_NUMBER is required" >&2
          exit 1
        fi

        if [ -z "${PR_BASE_SHA:-}" ] || [ -z "${PR_HEAD_SHA:-}" ]; then
          echo "PR_BASE_SHA and PR_HEAD_SHA are required" >&2
          exit 1
        fi

        echo "Computing changed files via git diff (PR base/head SHAs: ${PR_BASE_SHA}...${PR_HEAD_SHA})"
        echo "INCLUDE_REGEX=${INCLUDE_REGEX}"
        echo "EXCLUDE_REGEX=${EXCLUDE_REGEX}"

        # The default Actions checkout is shallow. Triple-dot diff requires a merge-base,
        # which may be missing unless we have enough history.
        if [ "$(git rev-parse --is-shallow-repository 2>/dev/null || echo false)" = "true" ]; then
          echo "Repository is shallow; fetching full history to enable merge-base computation"
          git fetch --no-tags --prune --unshallow origin || true
        fi

        # Ensure both SHAs are present locally.
        git fetch --no-tags --prune origin "${PR_BASE_SHA}" "${PR_HEAD_SHA}" || true

        merge_base="$(git merge-base "${PR_BASE_SHA}" "${PR_HEAD_SHA}" 2>/dev/null || true)"
        if [ -n "$merge_base" ]; then
          echo "Merge base: $merge_base"
          ALL_FILES="$(git diff --name-only --diff-filter=ACMRT "${merge_base}..${PR_HEAD_SHA}" || true)"
        else
          echo "Warning: no merge base found; falling back to two-dot diff (${PR_BASE_SHA}..${PR_HEAD_SHA})"
          ALL_FILES="$(git diff --name-only --diff-filter=ACMRT "${PR_BASE_SHA}..${PR_HEAD_SHA}" || true)"
        fi

        if [ -z "$ALL_FILES" ]; then
          echo "No files returned by git diff for PR #$PR_NUMBER" >&2
          exit 1
        fi

        FILTERED_FILES="$(printf '%s\n' "$ALL_FILES" \
          | grep -E "$INCLUDE_REGEX" \
          | grep -Ev "$EXCLUDE_REGEX" \
          | grep -E '\.(c|cpp|py)$' \
          | head -n "$MAX_FILES" || true)"

        if [ -z "$FILTERED_FILES" ]; then
          echo "No matching .c/.cpp/.py files after filters."
        fi

        printf '%s\n' "$FILTERED_FILES" > /tmp/changed_files.txt

        echo "CHANGED_FILES<<EOF" >> "$GITHUB_ENV"
        cat /tmp/changed_files.txt >> "$GITHUB_ENV"
        echo "EOF" >> "$GITHUB_ENV"

        echo "Selected files:"
        cat /tmp/changed_files.txt

    - name: Prepare build dependencies
      shell: bash
      run: |
        set -euo pipefail
        sudo apt-get update
        sudo apt-get install -y python3-pip gcc g++ jq
        python3 -m pip install --user --upgrade pip
        python3 -m pip install --user gcovr
        echo "$HOME/.local/bin" >> "$GITHUB_PATH"

    - name: Prepare machine (MsQuic)
      shell: bash
      run: |
        set -euo pipefail
        pwsh ./scripts/prepare-machine.ps1 -ForBuild -Tls "$TLS"

    - name: Run DeepTest agent
      shell: bash
      run: |
        set -euo pipefail

        if [ -z "${COPILOT_GITHUB_TOKEN:-}" ]; then
          echo "COPILOT_GITHUB_TOKEN secret is required" >&2
          exit 1
        fi

        if [ ! -f /tmp/changed_files.txt ] || [ ! -s /tmp/changed_files.txt ]; then
          echo "No matching .c/.cpp/.py files were selected; skipping DeepTest."
          printf '{"skipped": true, "reason": "no matching .c/.cpp/.py files"}\n' > /tmp/coverage_summary.json
          exit 0
        fi

        CHANGED_FILES_BULLETS="$(sed 's/^/- /' /tmp/changed_files.txt)"

        prompt="You are generating tests for PR #${PR_NUMBER} in ${GITHUB_REPOSITORY}."
        prompt+=$'\n\n'"Coverage target: ${COVERAGE_TARGET}% line coverage on the changed C/C++ files."
        prompt+=$'\n'"Max iterations: ${MAX_ITERATIONS} (stop early once the target is met)."
        prompt+=$'\n\n'"Changed files (filtered .c/.cpp/.py from this PR):"
        prompt+=$'\n'"${CHANGED_FILES_BULLETS}"
        prompt+=$'\n\n'"Build/test/coverage commands (Ubuntu):"
        prompt+=$'\n'"- Build with coverage: CC=gcc CXX=g++ CFLAGS='--coverage -O0 -g' CXXFLAGS='--coverage -O0 -g' LDFLAGS='--coverage' pwsh ./scripts/build.ps1 -Platform linux -Config ${CONFIG} -Arch ${ARCH} -Tls ${TLS} -DisablePerf -DisableTools -CI"
        prompt+=$'\n'"- Run tests: pwsh ./scripts/test.ps1 -Config ${CONFIG} -Arch ${ARCH} -Tls ${TLS} -GHA -LogProfile Full.Light"
        prompt+=$'\n'"- Generate coverage XML: gcovr --root . --object-directory build/linux/${ARCH}_${TLS} --filter '^src/' --exclude '^src/test/' --exclude '^submodules/' --exclude '^src/generated/' --cobertura-pretty --output ${COVERAGE_XML}"
        prompt+=$'\n\n'"Workflow:"
        prompt+=$'\n'"1. Generate or improve tests for the changed files to increase coverage."
        prompt+=$'\n'"2. Build with coverage instrumentation using the commands above."
        prompt+=$'\n'"3. Run the test suite."
        prompt+=$'\n'"4. Generate the Cobertura XML coverage report."
        prompt+=$'\n'"5. Parse the report to compute line coverage for the changed C/C++ files."
        prompt+=$'\n'"6. If coverage < ${COVERAGE_TARGET}%, go back to step 1 and add more tests."
        prompt+=$'\n'"7. Repeat until coverage >= ${COVERAGE_TARGET}% or you have done ${MAX_ITERATIONS} iterations."
        prompt+=$'\n\n'"Constraints:"
        prompt+=$'\n'"- Prefer adding tests under src/test/. Avoid changing production code unless required for testability."
        prompt+=$'\n'"- Keep tests deterministic; avoid flaky timing-dependent assertions."
        prompt+=$'\n'"- Do NOT create a PR, do NOT run 'gh pr create', do NOT push branches. The workflow will handle PR creation after you finish."
        prompt+=$'\n'"- After each coverage measurement, write a JSON summary to /tmp/coverage_summary.json with at least: {target, totals: {lines_valid, lines_covered, coverage_percent}, files: [{path, coverage_percent}]}."
        prompt+=$'\n\n'
        if [ -f .github/agentics/deeptest-coverage-ubuntu-pr.md ]; then
          prompt+="$(cat .github/agentics/deeptest-coverage-ubuntu-pr.md)"
        fi

        echo "Invoking ${AGENT_NAME} with ${COVERAGE_TARGET}% target for changed files"
        gh copilot --agent "$AGENT_NAME" --allow-all-tools -p "$prompt"

    - name: Request PR creation via safe output
      shell: bash
      run: |
        set -euo pipefail

        if [ -z "${COPILOT_GITHUB_TOKEN:-}" ]; then
          echo "COPILOT_GITHUB_TOKEN secret is required" >&2
          exit 1
        fi

        if ! git status --porcelain | grep -q .; then
          echo "No changes detected; requesting noop safe output."
          prompt="There are no file changes to commit. Please call the noop safe output tool with message: 'No test changes were generated for PR #${PR_NUMBER}.'"
          gh copilot --agent "$AGENT_NAME" --allow-all-tools -p "$prompt"
          exit 0
        fi

        title="[DeepTest+Coverage Ubuntu Run #${RUN_ID}] Add tests for coverage"
        changed_list="$(sed 's/^/- /' /tmp/changed_files.txt 2>/dev/null || echo '(none)')"
        coverage_info="$(cat /tmp/coverage_summary.json 2>/dev/null || echo 'Coverage summary not available.')"
        body="Generated/updated tests for PR #${PR_NUMBER}.\n\nChanged files considered:\n${changed_list}\n\nCoverage summary:\n${coverage_info}"

        prompt="IMPORTANT: You MUST use the 'create_pull_request' safe output tool to create a pull request. Do NOT use 'gh pr create' or 'git push' or any direct GitHub CLI commands.\n\nCall the create_pull_request safe output tool with these parameters:\n- title: ${title}\n- body: ${body}\n\nThis is a workflow safe output — the infrastructure will handle the actual PR creation."
        gh copilot --agent "$AGENT_NAME" --allow-all-tools -p "$prompt"

    - name: Upload coverage artifacts
      uses: actions/upload-artifact@v4
      with:
        name: ubuntu-gcovr-coverage
        path: |
          artifacts/coverage/*
          artifacts/TestResults/**
          artifacts/logs/**
          /tmp/changed_files.txt
          /tmp/coverage_summary.json

safe-outputs:
  create-pull-request:
    title-prefix: "[DeepTest+Coverage Ubuntu Run #${{ github.run_id }}] "
    labels: [automation, tests]
    draft: true
    if-no-changes: "warn"
  noop:


---
{{#runtime-import agentics/deeptest-coverage-ubuntu-pr.md}}
