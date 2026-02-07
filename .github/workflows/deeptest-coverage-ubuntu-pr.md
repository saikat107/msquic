---
description: Iteratively generate tests for PR-changed files until a coverage target is reached (Ubuntu + gcovr)
on:
  pull_request:
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
        description: "Target line coverage percentage for the changed files"
        required: false
        default: "100"
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
  COVERAGE_TARGET: ${{ github.event.inputs.coverage_target || '100' }}
  CONFIG: ${{ github.event.inputs.config || 'Debug' }}
  ARCH: ${{ github.event.inputs.arch || 'x64' }}
  TLS: ${{ github.event.inputs.tls || 'openssl' }}
  MAX_FILES: ${{ github.event.inputs.max_files || '10' }}
  INCLUDE_REGEX: ${{ github.event.inputs.include_regex || '^src/' }}
  EXCLUDE_REGEX: ${{ github.event.inputs.exclude_regex || '(\\.md$|\\.ya?ml$|\\.json$|\\.txt$|^docs/|^submodules/|^src/test/)' }}
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
        if [ -z "$head_ref" ] || [ "$head_ref" = "null" ] || [ -z "$base_ref" ] || [ "$base_ref" = "null" ]; then
          echo "Unable to determine PR head/base refs for PR #$PR_NUMBER" >&2
          exit 1
        fi

        echo "PR_HEAD_REF=$head_ref" >> "$GITHUB_ENV"
        echo "PR_BASE_REF=$base_ref" >> "$GITHUB_ENV"
        echo "PR head ref: $head_ref"
        echo "PR base ref: $base_ref"

    - name: Install Copilot CLI
      shell: bash
      run: |
        set -euo pipefail
        gh extension install github/gh-copilot || echo "Copilot CLI already installed"

    - name: Collect changed files from PR
      shell: bash
      run: |
        set -euo pipefail
        if [ -z "${PR_NUMBER:-}" ]; then
          echo "PR_NUMBER is required" >&2
          exit 1
        fi

        API_URL="${GITHUB_API_URL:-https://api.github.com}"
        echo "Fetching changed files for PR #$PR_NUMBER in $GITHUB_REPOSITORY via $API_URL"

        : > /tmp/changed_files_all.txt
        page=1
        while :; do
          resp="$(curl -fsSL \
            -H "Authorization: Bearer $GH_TOKEN" \
            -H "Accept: application/vnd.github+json" \
            -H "X-GitHub-Api-Version: 2022-11-28" \
            "$API_URL/repos/$GITHUB_REPOSITORY/pulls/$PR_NUMBER/files?per_page=100&page=$page")"

          count="$(printf '%s' "$resp" | jq 'length')"
          if [ "${count:-0}" -eq 0 ]; then
            break
          fi

          printf '%s' "$resp" | jq -r '.[].filename' >> /tmp/changed_files_all.txt
          page=$((page+1))
        done

        ALL_FILES="$(cat /tmp/changed_files_all.txt || true)"
        if [ -z "$ALL_FILES" ]; then
          echo "No files returned for PR #$PR_NUMBER" >&2
          exit 1
        fi

        FILTERED_FILES="$(printf '%s\n' "$ALL_FILES" \
          | grep -E "$INCLUDE_REGEX" \
          | grep -Ev "$EXCLUDE_REGEX" \
          | grep -E '\\.(c|h|cc|cpp|cxx|hpp|rs)$' \
          | head -n "$MAX_FILES" || true)"

        if [ -z "$FILTERED_FILES" ]; then
          echo "No matching source files after filters; falling back to first $MAX_FILES changed files."
          FILTERED_FILES="$(printf '%s\n' "$ALL_FILES" | head -n "$MAX_FILES" || true)"
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

    - name: Run DeepTest via Copilot CLI
      shell: bash
      run: |
        set -euo pipefail

        if [ -z "${COPILOT_GITHUB_TOKEN:-}" ]; then
          echo "COPILOT_GITHUB_TOKEN secret is required" >&2
          exit 1
        fi

        if [ ! -f .github/agentics/deeptest-coverage-ubuntu-pr.md ]; then
          echo "Missing base prompt file: .github/agentics/deeptest-coverage-ubuntu-pr.md" >&2
          exit 1
        fi

        echo "Invoking ${AGENT_NAME} for PR #${PR_NUMBER} (Run ID: ${RUN_ID})"
        CHANGED_FILES_BULLETS="$(sed 's/^/- /' /tmp/changed_files.txt)"
        prompt="You are generating tests for PR #${PR_NUMBER} in ${GITHUB_REPOSITORY}."$'\n\n'
        prompt+="Focus on these changed files (filtered, up to ${MAX_FILES}):"$'\n'
        prompt+="$CHANGED_FILES_BULLETS"$'\n\n'
        prompt+="Requirements:"$'\n'
        prompt+="- Follow MsQuic test patterns in src/test/. Prefer adding or updating focused tests for behavior changes."$'\n'
        prompt+="- Cover error paths and boundary conditions; avoid flaky timing-dependent tests."$'\n'
        prompt+="- Make minimal changes outside tests unless necessary for testability."$'\n'
        prompt+="- DeepTest agent should be used to generate test, build and run tests until the coverage reaches 100%."$'\n\n'
        prompt+="Build, test, coverage inputs (Ubuntu)"$'\n'
        prompt+="- build = \"pwsh ./scripts/build.ps1\""$'\n'
        prompt+="- test = \"pwsh ./scripts/test.ps1\""$'\n'
        prompt+="- coverage_result = \"./artifacts/coverage/gcovr-coverage.xml\""$'\n\n'
        prompt+="Coverage target: ${COVERAGE_TARGET}% (changed files)"$'\n'
        prompt+="Max iterations: ${MAX_ITERATIONS}"$'\n'
        prompt+="Keep adding tests until you get 100% coverage on the files changed in the PR."$'\n\n'
        prompt+="$(cat .github/agentics/deeptest-coverage-ubuntu-pr.md)"$'\n\n'
        prompt+="Notes:"$'\n'
        prompt+="- The changed file list is also at: /tmp/changed_files.txt"$'\n'
        prompt+="- You should run build/test with coverage, check ${COVERAGE_XML}, and repeat until the target is reached (or you hit the max-iteration limit)."$'\n'
        prompt+="- When you are done, create a PR using the workflow safe output."$'\n'

        gh copilot --agent "$AGENT_NAME" --allow-all-tools -p "$prompt"

        if ! git status --porcelain | grep -q .; then
          echo "No changes detected after agent."
        fi

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
---

{{#runtime-import agentics/deeptest-coverage-ubuntu-pr.md}}
