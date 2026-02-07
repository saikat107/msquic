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

    - name: Iterate until coverage target
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

        changed_files="$(cat /tmp/changed_files.txt)"
        max_iter="${MAX_ITERATIONS}"
        target="${COVERAGE_TARGET}"

        mkdir -p artifacts/coverage

        iter=0
        last_summary_json=""
        while :; do
          echo "--- Build & test (iteration $iter) ---"

          export CC=gcc
          export CXX=g++
          export CFLAGS="--coverage -O0 -g"
          export CXXFLAGS="--coverage -O0 -g"
          export LDFLAGS="--coverage"

          pwsh ./scripts/build.ps1 -Platform linux -Config "$CONFIG" -Arch "$ARCH" -Tls "$TLS" -DisablePerf -DisableTools -CI

          OBJDIR="build/linux/${ARCH}_${TLS}"
          if [ -d "$OBJDIR" ]; then
            find "$OBJDIR" -name '*.gcda' -delete || true
          fi

          pwsh ./scripts/test.ps1 -Config "$CONFIG" -Arch "$ARCH" -Tls "$TLS" -GHA -LogProfile Full.Light

          echo "--- Generate coverage report (gcovr) ---"
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
            --output "$COVERAGE_XML"

          python3 - <<'PY' > /tmp/coverage_summary.json
          import json
          import os
          import xml.etree.ElementTree as ET

          coverage_xml = os.environ.get('COVERAGE_XML')
          target_files_path = '/tmp/changed_files.txt'

          def pct(covered, total):
              return round((covered / total) * 100.0, 2) if total else 0.0

          tree = ET.parse(coverage_xml)
          root = tree.getroot()

          cov_by_file = {}
          for cls in root.findall('.//class'):
              filename = (cls.get('filename') or '').replace('\\\\', '/').replace('\\', '/')
              if not filename:
                  continue
              covered = 0
              total = 0
              for line in cls.findall('.//line'):
                  total += 1
                  hits = int(line.get('hits') or '0')
                  if hits > 0:
                      covered += 1
              ent = cov_by_file.get(filename)
              if ent is None:
                  cov_by_file[filename] = {'covered': covered, 'total': total}
              else:
                  ent['covered'] += covered
                  ent['total'] += total

          def resolve_key(changed):
              changed = changed.replace('\\\\', '/').replace('\\', '/').strip()
              if changed in cov_by_file:
                  return changed
              suffix = '/' + changed
              candidates = [k for k in cov_by_file.keys() if k.endswith(suffix)]
              return min(candidates, key=len) if candidates else None

          with open(target_files_path, 'r', encoding='utf-8') as f:
              changed_files_list = [ln.strip() for ln in f.readlines() if ln.strip()]

          per_file = []
          sum_cov = 0
          sum_total = 0
          for fpath in changed_files_list:
              key = resolve_key(fpath)
              if key is None:
                  per_file.append({'file': fpath, 'found': False, 'covered': 0, 'total': 0, 'percent': 0.0})
                  continue
              covered = int(cov_by_file[key]['covered'])
              total = int(cov_by_file[key]['total'])
              per_file.append({'file': fpath, 'found': True, 'covered': covered, 'total': total, 'percent': pct(covered, total)})
              sum_cov += covered
              sum_total += total

          out = {
              'aggregatePercent': pct(sum_cov, sum_total),
              'covered': sum_cov,
              'total': sum_total,
              'perFile': sorted(per_file, key=lambda x: (x['percent'], x['file']))
          }
          print(json.dumps(out))
          PY

          last_summary_json="$(cat /tmp/coverage_summary.json)"
          agg="$(printf '%s' "$last_summary_json" | jq -r '.aggregatePercent')"
          covered="$(printf '%s' "$last_summary_json" | jq -r '.covered')"
          total="$(printf '%s' "$last_summary_json" | jq -r '.total')"
          echo "Changed-files coverage: ${agg}% (${covered}/${total} covered)"

          below_count="$(printf '%s' "$last_summary_json" | jq --argjson t "$target" '[.perFile[] | select(.percent < $t)] | length')"
          if [ "$below_count" -eq 0 ]; then
            echo "Coverage target reached for changed files (>= ${target}%)."
            break
          fi

          if [ "$iter" -ge "$max_iter" ]; then
            echo "Reached max iterations (${max_iter}). Stopping."
            break
          fi

          echo "--- Invoking agent (${AGENT_NAME}) ---"
          bullets="$(printf '%s\n' "$changed_files" | sed 's/^/- /')"
          coverage_lines="$(printf '%s' "$last_summary_json" | jq -r '.perFile[] | "- \(.file): \(.percent)%" + (if .found then "" else " (not found in report)" end)')"

          prompt="$(cat .github/agentics/deeptest-coverage-ubuntu-pr.md)"
          prompt+=$'\n\n'
          prompt+="Repository: ${GITHUB_REPOSITORY}"$'\n'
          prompt+="PR: #${PR_NUMBER}"$'\n'
          prompt+="Workflow run: ${RUN_ID}"$'\n'
          prompt+="Agent: ${AGENT_NAME}"$'\n'
          prompt+="Iteration: ${iter} / ${max_iter}"$'\n'
          prompt+="Coverage target: ${target}% (changed files)"$'\n\n'
          prompt+="Changed files:"$'\n'
          prompt+="$bullets"$'\n\n'
          prompt+="Current changed-files coverage summary:"$'\n'
          prompt+="$coverage_lines"$'\n\n'
          prompt+="Coverage report: ${COVERAGE_XML}"$'\n\n'
          prompt+="Now update/add tests to improve coverage for the files below target. Prefer src/test/ changes. When you are done, create a PR using the workflow safe output."

          gh copilot --agent "$AGENT_NAME" --allow-all-tools -p "$prompt"

          if ! git status --porcelain | grep -q .; then
            echo "No changes detected after agent; stopping."
            break
          fi

          iter=$((iter+1))
        done

        if [ -n "$last_summary_json" ]; then
          echo "FINAL_CHANGED_FILES_COVERAGE=$(printf '%s' "$last_summary_json" | jq -r '.aggregatePercent')" >> "$GITHUB_ENV"
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
