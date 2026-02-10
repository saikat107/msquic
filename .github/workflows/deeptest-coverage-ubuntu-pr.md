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
        description: "Target line coverage percentage for the changed C/C++ files"
        required: false
        default: "90"
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
  COVERAGE_TARGET: ${{ github.event.inputs.coverage_target || '90' }}
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
          | grep -E '\\.(c|cpp|py)$' \
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

    - name: Iterate DeepTest + coverage to target
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

        if [ ! -f /tmp/changed_files.txt ]; then
          echo "Missing changed file list: /tmp/changed_files.txt" >&2
          exit 1
        fi

        if [ ! -s /tmp/changed_files.txt ]; then
          echo "No matching .c/.cpp/.py files were selected; skipping DeepTest + build/test/coverage."
          printf '{"skipped": true, "reason": "no matching .c/.cpp/.py files"}\n' > /tmp/coverage_summary.json
          exit 0
        fi

        changed_c_files="$(grep -E '\\.(c|cpp)$' /tmp/changed_files.txt || true)"

        i=1
        while [ "$i" -le "$MAX_ITERATIONS" ]; do
          echo "=== Iteration ${i}/${MAX_ITERATIONS} (target: ${COVERAGE_TARGET}%) ==="

          CHANGED_FILES_BULLETS="$(sed 's/^/- /' /tmp/changed_files.txt)"
          prompt="You are generating tests for PR #${PR_NUMBER} in ${GITHUB_REPOSITORY}."$'\n\n'
          prompt+="Iteration ${i} of ${MAX_ITERATIONS}."$'\n'
          prompt+="Coverage target: ${COVERAGE_TARGET}% (changed C/C++ files)."$'\n\n'
          prompt+="Focus on these changed files (filtered, up to ${MAX_FILES}):"$'\n'
          prompt+="$CHANGED_FILES_BULLETS"$'\n\n'
          if [ -f /tmp/coverage_summary.json ]; then
            prompt+="Current coverage summary (from previous iteration):"$'\n'
            prompt+="$(cat /tmp/coverage_summary.json)"$'\n\n'
          fi
          prompt+="Requirements:"$'\n'
          prompt+="- First, use DeepTest skills to generate or improve tests."$'\n'
          prompt+="- Prefer changes under src/test/. Avoid changing production code unless required for testability."$'\n'
          prompt+="- Keep tests deterministic; avoid flaky timing-dependent assertions."$'\n'
          prompt+="- Do NOT create a PR; the workflow will create a branch and PR if there are changes."$'\n\n'
          prompt+="Build, test, coverage inputs (Ubuntu):"$'\n'
          prompt+="- build = \"pwsh ./scripts/build.ps1 -Platform linux -Config ${CONFIG} -Arch ${ARCH} -Tls ${TLS} -DisablePerf -DisableTools -CI\""$'\n'
          prompt+="- test = \"pwsh ./scripts/test.ps1 -Config ${CONFIG} -Arch ${ARCH} -Tls ${TLS} -GHA -LogProfile Full.Light\""$'\n'
          prompt+="- coverage_result = \"./${COVERAGE_XML}\""$'\n\n'
          prompt+="$(cat .github/agentics/deeptest-coverage-ubuntu-pr.md)"$'\n'

          echo "Invoking ${AGENT_NAME} (iteration ${i})"
          gh copilot --agent "$AGENT_NAME" --allow-all-tools -p "$prompt"

          echo "Building with coverage instrumentation"
          export CC=gcc
          export CXX=g++
          export CFLAGS="--coverage -O0 -g"
          export CXXFLAGS="--coverage -O0 -g"
          export LDFLAGS="--coverage"
          pwsh ./scripts/build.ps1 -Platform linux -Config "$CONFIG" -Arch "$ARCH" -Tls "$TLS" -DisablePerf -DisableTools -CI

          echo "Running tests"
          pwsh ./scripts/test.ps1 -Config "$CONFIG" -Arch "$ARCH" -Tls "$TLS" -GHA -LogProfile Full.Light

          echo "Generating coverage report (gcovr)"
          mkdir -p artifacts/coverage
          OBJDIR="build/linux/${ARCH}_${TLS}"
          gcovr --root . \
            --object-directory "$OBJDIR" \
            --filter '^src/' \
            --exclude '^src/test/' \
            --exclude '^submodules/' \
            --exclude '^src/generated/' \
            --cobertura-pretty \
            --output "${COVERAGE_XML}"

          echo "Computing changed-files coverage from Cobertura XML"
          python3 - <<'PY'
          import json
          import os
          import sys
          import xml.etree.ElementTree as ET

          coverage_xml = os.environ.get('COVERAGE_XML', 'artifacts/coverage/gcovr-coverage.xml')
          target = float(os.environ.get('COVERAGE_TARGET', '90'))

          changed_files_path = '/tmp/changed_files.txt'
          with open(changed_files_path, 'r', encoding='utf-8') as f:
            changed_files = [line.strip() for line in f if line.strip()]

          changed_cc = [p.replace('\\', '/') for p in changed_files if p.endswith('.c') or p.endswith('.cpp')]

          summary = {
            'target': target,
            'changed_files': changed_files,
            'changed_cc_files': changed_cc,
            'files': [],
            'totals': {'lines_valid': 0, 'lines_covered': 0, 'coverage_percent': 100.0},
          }

          if not changed_cc:
            # No C/C++ files to measure with gcovr; treat as satisfied.
            with open('/tmp/coverage_summary.json', 'w', encoding='utf-8') as out:
              json.dump(summary, out, indent=2)
            print('CHANGED_FILES_COVERAGE=100.0')
            sys.exit(0)

          tree = ET.parse(coverage_xml)
          root = tree.getroot()

          # Map filename -> (covered, valid)
          file_stats = {}
          for cls in root.findall('.//class'):
            filename = cls.attrib.get('filename')
            if not filename:
              continue
            filename = filename.replace('\\', '/')
            covered = 0
            valid = 0
            for line in cls.findall('.//line'):
              valid += 1
              try:
                hits = int(line.attrib.get('hits', '0'))
              except ValueError:
                hits = 0
              if hits > 0:
                covered += 1
            prev = file_stats.get(filename)
            if prev:
              covered += prev[0]
              valid += prev[1]
            file_stats[filename] = (covered, valid)

          def find_best_match(path: str):
            # Cobertura filenames are typically repo-relative. Try exact match first.
            if path in file_stats:
              return path
            # Try suffix match if gcovr emitted a different root prefix.
            matches = [k for k in file_stats.keys() if k.endswith(path)]
            if len(matches) == 1:
              return matches[0]
            # As a fallback, try matching by basename.
            base = path.split('/')[-1]
            matches = [k for k in file_stats.keys() if k.split('/')[-1] == base]
            if len(matches) == 1:
              return matches[0]
            return None

          total_cov = 0
          total_valid = 0
          for p in changed_cc:
            match = find_best_match(p)
            if not match:
              summary['files'].append({'path': p, 'matched': None, 'lines_valid': 0, 'lines_covered': 0, 'coverage_percent': None})
              continue
            cov, valid = file_stats.get(match, (0, 0))
            percent = (100.0 * cov / valid) if valid else None
            summary['files'].append({'path': p, 'matched': match, 'lines_valid': valid, 'lines_covered': cov, 'coverage_percent': percent})
            if valid:
              total_cov += cov
              total_valid += valid

          overall = (100.0 * total_cov / total_valid) if total_valid else 0.0
          summary['totals'] = {'lines_valid': total_valid, 'lines_covered': total_cov, 'coverage_percent': overall}

          with open('/tmp/coverage_summary.json', 'w', encoding='utf-8') as out:
            json.dump(summary, out, indent=2)

          print(f'CHANGED_FILES_COVERAGE={overall:.2f}')
          PY

          coverage="$(python3 -c "import json; print(json.load(open('/tmp/coverage_summary.json'))['totals']['coverage_percent'])")"
          echo "Changed-files coverage: ${coverage}%"

          meets="$(python3 -c "import json, os; cov=float(json.load(open('/tmp/coverage_summary.json'))['totals']['coverage_percent']); tgt=float(os.environ.get('COVERAGE_TARGET','90')); print('true' if cov>=tgt else 'false')")"
          if [ "$meets" = "true" ]; then
            echo "Coverage target met."
            break
          fi

          i=$((i+1))
        done

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
          prompt="No changes were made for PR #${PR_NUMBER} in ${GITHUB_REPOSITORY}. Please call the noop safe output tool with a short status message."
          gh copilot --agent "$AGENT_NAME" --allow-all-tools -p "$prompt"
          exit 0
        fi

        title="[DeepTest+Coverage Ubuntu Run #${RUN_ID}] Add tests for coverage"
        body="Generated/updated tests for PR #${PR_NUMBER}.\n\nChanged files considered:\n$(sed 's/^/- /' /tmp/changed_files.txt || true)\n\nCoverage summary:\n$(cat /tmp/coverage_summary.json || true)"

        prompt="Create a draft pull request for the changes you generated in this workflow run using the workflow safe output.\n\nTitle: ${title}\n\nBody:\n${body}\n"
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
