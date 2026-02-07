---
description: Iteratively generate tests for PR-changed files until a coverage target is reached (Windows + OpenCppCoverage)
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
      test_filter:
        description: "GoogleTest filter passed to scripts/test.ps1 -Filter"
        required: false
        default: "BbrTest"
        type: string
      config:
        description: "Build/test configuration"
        required: false
        default: "Release"
        type: string
      arch:
        description: "CPU architecture"
        required: false
        default: "x64"
        type: string
      tls:
        description: "TLS provider"
        required: false
        default: "schannel"
        type: string
      max_files:
        description: "Max number of changed files to include (for logging/artifacts)"
        required: false
        default: "10"
        type: string
      include_regex:
        description: "Regex include filter (applied to path list)"
        required: false
        default: "^src/"
        type: string
      exclude_regex:
        description: "Regex exclude filter (applied to path list)"
        required: false
        default: "(\\.md$|\\.ya?ml$|\\.json$|\\.txt$|^docs/|^submodules/|^src/test/)"
        type: string

permissions:
  contents: read
  issues: read
  pull-requests: read

strict: false

runs-on: windows-latest

env:
  GH_TOKEN: ${{ github.token }}
  COPILOT_GITHUB_TOKEN: ${{ secrets.COPILOT_GITHUB_TOKEN }}
  RUN_ID: ${{ github.run_id }}
  PR_NUMBER: ${{ github.event.pull_request.number || github.event.inputs.pr_number }}
  AGENT_NAME: ${{ github.event.inputs.agent_name || 'DeepTest' }}
  MAX_ITERATIONS: ${{ github.event.inputs.max_iterations || '5' }}
  COVERAGE_TARGET: ${{ github.event.inputs.coverage_target || '100' }}
  TEST_FILTER: ${{ github.event.inputs.test_filter || 'BbrTest' }}
  CONFIG: ${{ github.event.inputs.config || 'Release' }}
  ARCH: ${{ github.event.inputs.arch || 'x64' }}
  TLS: ${{ github.event.inputs.tls || 'schannel' }}
  MAX_FILES: ${{ github.event.inputs.max_files || '10' }}
  INCLUDE_REGEX: ${{ github.event.inputs.include_regex || '^src/' }}
  EXCLUDE_REGEX: ${{ github.event.inputs.exclude_regex || '(\\.md$|\\.ya?ml$|\\.json$|\\.txt$|^docs/|^submodules/|^src/test/)' }}
  PR_HEAD_REF: ${{ github.event.pull_request.head.ref || '' }}
  PR_BASE_REF: ${{ github.event.pull_request.base.ref || '' }}
  COVERAGE_XML: artifacts\\coverage\\msquiccoverage.xml

engine:
  id: custom
  steps:
    - name: Fetch PR metadata
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'
        if ([string]::IsNullOrWhiteSpace($env:PR_NUMBER)) {
          throw 'PR_NUMBER is required'
        }

        $apiUrl = if ($env:GITHUB_API_URL) { $env:GITHUB_API_URL } else { 'https://api.github.com' }
        $headers = @{
          Authorization = "Bearer $env:GH_TOKEN"
          Accept        = 'application/vnd.github+json'
          'X-GitHub-Api-Version' = '2022-11-28'
        }

        $prUri = "$apiUrl/repos/$env:GITHUB_REPOSITORY/pulls/$env:PR_NUMBER"
        $pr = Invoke-RestMethod -Uri $prUri -Headers $headers -Method Get

        $headRef = [string]$pr.head.ref
        $baseRef = [string]$pr.base.ref
        if ([string]::IsNullOrWhiteSpace($headRef) -or [string]::IsNullOrWhiteSpace($baseRef)) {
          throw "Unable to determine PR head/base refs for PR #$($env:PR_NUMBER)"
        }

        "PR_HEAD_REF=$headRef" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
        "PR_BASE_REF=$baseRef" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
        Write-Host "PR head ref: $headRef"
        Write-Host "PR base ref: $baseRef"

    - name: Install Copilot CLI
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Continue'
        gh extension install github/gh-copilot

    - name: Collect changed files from PR
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'
        if ([string]::IsNullOrWhiteSpace($env:PR_NUMBER)) {
          throw 'PR_NUMBER is required'
        }

        $apiUrl = if ($env:GITHUB_API_URL) { $env:GITHUB_API_URL } else { 'https://api.github.com' }
        Write-Host "Fetching changed files for PR #$($env:PR_NUMBER) in $env:GITHUB_REPOSITORY via $apiUrl"

        $headers = @{
          Authorization = "Bearer $env:GH_TOKEN"
          Accept        = 'application/vnd.github+json'
        }

        $all = New-Object System.Collections.Generic.List[string]
        $page = 1
        while ($true) {
          $uri = "$apiUrl/repos/$env:GITHUB_REPOSITORY/pulls/$env:PR_NUMBER/files?per_page=100&page=$page"
          $resp = Invoke-RestMethod -Uri $uri -Headers $headers -Method Get
          if ($null -eq $resp -or $resp.Count -eq 0) { break }
          foreach ($f in $resp) { if ($f.filename) { $all.Add([string]$f.filename) } }
          $page++
        }

        if ($all.Count -eq 0) {
          throw "No files returned for PR #$($env:PR_NUMBER)"
        }

        $filtered = $all |
          Where-Object { $_ -match $env:INCLUDE_REGEX } |
          Where-Object { $_ -notmatch $env:EXCLUDE_REGEX } |
          Where-Object { $_ -match '\\.(c|h|cc|cpp|cxx|hpp|rs)$' } |
          Select-Object -First ([int]$env:MAX_FILES)

        if (-not $filtered -or $filtered.Count -eq 0) {
          Write-Host "No matching source files after filters; falling back to first $($env:MAX_FILES) changed files."
          $filtered = $all | Select-Object -First ([int]$env:MAX_FILES)
        }

        $outPath = Join-Path $env:RUNNER_TEMP 'changed_files.txt'
        $filtered | Set-Content -Path $outPath -Encoding utf8

        "CHANGED_FILES<<EOF" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
        Get-Content -Path $outPath | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
        "EOF" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append

        Write-Host 'Selected files:'
        Get-Content -Path $outPath

    - name: Prepare machine (installs OpenCppCoverage)
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'
        scripts\prepare-machine.ps1 -ForBuild -Tls $env:TLS -InstallCodeCoverage

    - name: Run DeepTest via Copilot CLI
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'

        if ([string]::IsNullOrWhiteSpace($env:COPILOT_GITHUB_TOKEN)) {
          throw 'COPILOT_GITHUB_TOKEN secret is required'
        }

        $changedFilesPath = Join-Path $env:RUNNER_TEMP 'changed_files.txt'
        if (-not (Test-Path $changedFilesPath)) {
          throw "Missing changed files list at $changedFilesPath"
        }

        $basePromptPath = Join-Path $PWD '.github\agentics\deeptest-coverage-windows-pr.md'
        if (-not (Test-Path $basePromptPath)) {
          throw "Missing base prompt file at $basePromptPath"
        }
        $basePrompt = Get-Content -Path $basePromptPath -Raw

        $changedFiles = Get-Content -Path $changedFilesPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        $bullets = ($changedFiles | ForEach-Object { "- $_" }) -join "`n"

        Write-Host "Invoking $($env:AGENT_NAME) for PR #$($env:PR_NUMBER) (Run ID: $($env:RUN_ID))"

        $prompt = @(
          "You are generating tests for PR #$($env:PR_NUMBER) in $($env:GITHUB_REPOSITORY).",
          '',
          "Focus on these changed files (filtered, up to $($env:MAX_FILES)):",
          $bullets,
          '',
          'Requirements:',
          '- Follow MsQuic test patterns in src/test/. Prefer adding or updating focused tests for behavior changes.',
          '- Cover error paths and boundary conditions; avoid flaky timing-dependent tests.',
          '- Make minimal changes outside tests unless necessary for testability.',
          '- DeepTest agent should be used to generate test, build and run tests until the coverage reaches 100%.',
          '',
          'Build, test, coverage inputs (Windows)',
          '- build = ".\scripts\build.ps1"',
          "- test = \".\scripts\test.ps1 -Filter $($env:TEST_FILTER) -CodeCoverage\"",
          '- coverage_result = ".\artifacts\coverage\msquiccoverage.xml"',
          '',
          "Coverage target: $($env:COVERAGE_TARGET)% (changed files)",
          "Max iterations: $($env:MAX_ITERATIONS)",
          'Keep adding tests until you get 100% coverage on the files changed in the PR.',
          '',
          $basePrompt.TrimEnd(),
          '',
          'Notes:',
          "- The changed file list is also at: $changedFilesPath",
          "- You should run build/test with coverage, check .\\$($env:COVERAGE_XML), and repeat until the target is reached (or you hit the max-iteration limit).",
          '- When you are done, create a PR using the workflow safe output.'
        ) -join "`n"

        gh copilot --agent $env:AGENT_NAME --allow-all-tools -p "$prompt"

        $status = git status --porcelain
        if ([string]::IsNullOrWhiteSpace($status)) {
          Write-Host 'No changes detected after agent.'
        }

    - name: Upload coverage artifacts
      uses: actions/upload-artifact@v4
      with:
        name: windows-opencppcoverage
        path: |
          artifacts\\coverage\\*.xml
          artifacts\\coverage\\*.html
          artifacts\\TestResults\\**
          artifacts\\logs\\**
          ${{ runner.temp }}\\changed_files.txt

safe-outputs:
  create-pull-request:
    title-prefix: "[DeepTest+Coverage Windows Run #${{ github.run_id }}] "
    labels: [automation, tests]
    draft: true
    if-no-changes: "warn"
---

{{#runtime-import agentics/deeptest-coverage-windows-pr.md}}
