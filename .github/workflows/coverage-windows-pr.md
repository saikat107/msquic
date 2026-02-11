---
description: Run Windows PR tests with OpenCppCoverage and upload coverage artifacts
on:
  workflow_dispatch:
    inputs:
      pr_number:
        description: "Pull request number (required when manually dispatching)"
        required: true
        type: string
      test_filter:
        description: "GoogleTest filter passed to scripts/test.ps1 -Filter"
        required: false
        default: "*"
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
  TEST_FILTER: ${{ github.event.inputs.test_filter || '*' }}
  CONFIG: ${{ github.event.inputs.config || 'Release' }}
  ARCH: ${{ github.event.inputs.arch || 'x64' }}
  TLS: ${{ github.event.inputs.tls || 'schannel' }}
  MAX_FILES: ${{ github.event.inputs.max_files || '10' }}
  INCLUDE_REGEX: ${{ github.event.inputs.include_regex || '^src/' }}
  EXCLUDE_REGEX: ${{ github.event.inputs.exclude_regex || '(\\.md$|\\.ya?ml$|\\.json$|\\.txt$|^docs/|^submodules/|^src/test/)' }}
  PR_HEAD_REF: ${{ github.event.pull_request.head.ref || '' }}
  PR_BASE_REF: ${{ github.event.pull_request.base.ref || '' }}

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

    - name: Run DeepTest (generate/update tests in working tree)
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

        $bullets = (Get-Content -Path $changedFilesPath | ForEach-Object { "- $_" }) -join "`n"
        $prompt = @(
          "You are generating and/or updating tests for PR #$($env:PR_NUMBER) in $($env:GITHUB_REPOSITORY).",
          "",
          "Focus on these changed files (filtered, up to $($env:MAX_FILES)):",
          $bullets,
          "",
          "Requirements:",
          "- Follow MsQuic test patterns in src/test/. Prefer focused unit/functional tests.",
          "- Cover error paths and boundary conditions; avoid flaky timing-dependent tests.",
          "- Keep changes minimal outside src/test/ unless needed for testability.",
          "- Run locally in this workflow checkout; do not push commits.",
          "- Create a PR with all test changes using the workflow's safe output (create pull request); do NOT run gh pr create.",
          "- Include workflow run ID $($env:RUN_ID) in the PR title."
        ) -join "`n"

        gh copilot --agent DeepTest --allow-all-tools -p "$prompt"

    - name: Detect generated changes
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'
        $status = git status --porcelain
        if ([string]::IsNullOrWhiteSpace($status)) {
          "HAS_CHANGES=false" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
          Write-Host 'No changes detected after DeepTest.'
        } else {
          "HAS_CHANGES=true" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
          Write-Host 'Changes detected after DeepTest:'
          git status --porcelain
        }

    - name: Prepare machine (installs OpenCppCoverage)
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'
        scripts\prepare-machine.ps1 -ForBuild -Tls $env:TLS -InstallCodeCoverage

    - name: Build
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'
        scripts\build.ps1 -Config $env:CONFIG -Arch $env:ARCH -Tls $env:TLS -CI

    - name: Run tests with coverage (OpenCppCoverage)
      shell: pwsh
      run: |
        $ErrorActionPreference = 'Stop'
        scripts\test.ps1 -Filter $env:TEST_FILTER -Config $env:CONFIG -Arch $env:ARCH -Tls $env:TLS -CodeCoverage -GHA -GenerateXmlResults -LogProfile Full.Light

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
  noop:

---

{{#runtime-import agentics/coverage-windows-pr.md}}
