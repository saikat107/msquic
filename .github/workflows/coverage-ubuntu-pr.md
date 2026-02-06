---
description: Run Ubuntu PR tests with GCC/gcov coverage instrumentation and upload gcovr reports
on:
  pull_request:
    types: [opened, synchronize, reopened, ready_for_review]
  workflow_dispatch:
    inputs:
      pr_number:
        description: "Pull request number (required when manually dispatching)"
        required: true
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
        description: "Max number of changed files to include (for logging/artifacts)"
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
  PR_NUMBER: ${{ github.event.pull_request.number || github.event.inputs.pr_number }}
  CONFIG: ${{ github.event.inputs.config || 'Debug' }}
  ARCH: ${{ github.event.inputs.arch || 'x64' }}
  TLS: ${{ github.event.inputs.tls || 'openssl' }}
  MAX_FILES: ${{ github.event.inputs.max_files || '10' }}
  INCLUDE_REGEX: ${{ github.event.inputs.include_regex || '^src/' }}
  EXCLUDE_REGEX: ${{ github.event.inputs.exclude_regex || '(\\.md$|\\.ya?ml$|\\.json$|\\.txt$|^docs/|^submodules/|^src/test/)' }}

engine:
  id: custom
  steps:
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
        sudo apt-get install -y python3-pip gcov
        python3 -m pip install --user --upgrade pip
        python3 -m pip install --user gcovr
        echo "$HOME/.local/bin" >> "$GITHUB_PATH"

    - name: Prepare machine (MsQuic)
      shell: bash
      run: |
        set -euo pipefail
        pwsh ./scripts/prepare-machine.ps1 -ForBuild -Tls "$TLS"

    - name: Build with coverage instrumentation (GCC/gcov)
      shell: bash
      run: |
        set -euo pipefail
        export CC=gcc
        export CXX=g++
        export CFLAGS="--coverage -O0 -g"
        export CXXFLAGS="--coverage -O0 -g"
        export LDFLAGS="--coverage"
        pwsh ./scripts/build.ps1 -Platform linux -Config "$CONFIG" -Arch "$ARCH" -Tls "$TLS" -DisablePerf -DisableTools -CI

    - name: Run tests (generates .gcda)
      shell: bash
      run: |
        set -euo pipefail
        pwsh ./scripts/test.ps1 -Config "$CONFIG" -Arch "$ARCH" -Tls "$TLS" -GHA -LogProfile Full.Light

    - name: Generate coverage report (gcovr)
      shell: bash
      run: |
        set -euo pipefail
        mkdir -p artifacts/coverage
        OBJDIR="build/linux/${ARCH}_${TLS}"
        echo "Using object directory: $OBJDIR"

        gcovr --version

        gcovr \
          --root . \
          --object-directory "$OBJDIR" \
          --filter '^src/' \
          --exclude '^src/test/' \
          --exclude '^submodules/' \
          --exclude '^src/generated/' \
          --cobertura-pretty \
          --output artifacts/coverage/gcovr-coverage.xml

        gcovr \
          --root . \
          --object-directory "$OBJDIR" \
          --filter '^src/' \
          --exclude '^src/test/' \
          --exclude '^submodules/' \
          --exclude '^src/generated/' \
          --html-details artifacts/coverage/gcovr-coverage.html

        echo "Coverage outputs:" 
        ls -la artifacts/coverage

    - name: Upload coverage artifacts
      uses: actions/upload-artifact@v4
      with:
        name: ubuntu-gcovr-coverage
        path: |
          artifacts/coverage/*
          artifacts/TestResults/**
          artifacts/logs/**
          /tmp/changed_files.txt

---

{{#runtime-import agentics/coverage-ubuntu-pr.md}}
