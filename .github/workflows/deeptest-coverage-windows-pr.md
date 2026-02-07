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

    - name: Iterate until coverage target
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

        function Get-CoberturaFileCoverage {
          param(
            [Parameter(Mandatory=$true)][string]$XmlPath
          )
          if (-not (Test-Path $XmlPath)) { return @{} }

          [xml]$xml = Get-Content -Path $XmlPath
          $coverage = @{}

          foreach ($package in @($xml.coverage.packages.package)) {
            foreach ($class in @($package.classes.class)) {
              $filename = [string]$class.filename
              if ([string]::IsNullOrWhiteSpace($filename)) { continue }
              $filename = $filename -replace '\\', '/'

              $linesCovered = 0
              $linesTotal = 0
              foreach ($line in @($class.lines.line)) {
                $linesTotal++
                if ([int]$line.hits -gt 0) { $linesCovered++ }
              }

              if (-not $coverage.ContainsKey($filename)) {
                $coverage[$filename] = [pscustomobject]@{ LinesCovered=$linesCovered; LinesTotal=$linesTotal }
              } else {
                $coverage[$filename].LinesCovered += $linesCovered
                $coverage[$filename].LinesTotal += $linesTotal
              }
            }
          }
          return $coverage
        }

        function Resolve-CoverageKey {
          param(
            [Parameter(Mandatory=$true)][hashtable]$CoverageByFile,
            [Parameter(Mandatory=$true)][string]$ChangedFile
          )
          $changed = ($ChangedFile -replace '\\', '/').Trim()
          if ($CoverageByFile.ContainsKey($changed)) { return $changed }

          $candidates = $CoverageByFile.Keys | Where-Object { $_.EndsWith('/' + $changed) }
          if ($candidates -and $candidates.Count -gt 0) {
            return ($candidates | Sort-Object Length | Select-Object -First 1)
          }
          return $null
        }

        function Get-ChangedFilesCoverageSummary {
          param(
            [Parameter(Mandatory=$true)][string]$XmlPath,
            [Parameter(Mandatory=$true)][string[]]$ChangedFiles
          )
          $covByFile = Get-CoberturaFileCoverage -XmlPath $XmlPath
          $perFile = @()
          $sumCovered = 0
          $sumTotal = 0

          foreach ($f in $ChangedFiles) {
            $key = if ($covByFile.Count -gt 0) { Resolve-CoverageKey -CoverageByFile $covByFile -ChangedFile $f } else { $null }
            if ($null -eq $key) {
              $perFile += [pscustomobject]@{ File=$f; Covered=0; Total=0; Percent=0; Found=$false }
              continue
            }
            $covered = [int]$covByFile[$key].LinesCovered
            $total = [int]$covByFile[$key].LinesTotal
            $pct = if ($total -gt 0) { [math]::Round(($covered / $total) * 100, 2) } else { 0 }
            $perFile += [pscustomobject]@{ File=$f; Covered=$covered; Total=$total; Percent=$pct; Found=$true }
            $sumCovered += $covered
            $sumTotal += $total
          }

          $aggPct = if ($sumTotal -gt 0) { [math]::Round(($sumCovered / $sumTotal) * 100, 2) } else { 0 }
          return [pscustomobject]@{ AggregatePercent=$aggPct; Covered=$sumCovered; Total=$sumTotal; PerFile=$perFile }
        }

        $changedFiles = Get-Content -Path $changedFilesPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        $maxIterations = [int]$env:MAX_ITERATIONS
        $target = [double]$env:COVERAGE_TARGET
        $coverageXml = Join-Path $PWD $env:COVERAGE_XML

        $currentIteration = 0
        $lastSummary = $null

        while ($true) {
          Write-Host "--- Build & test (iteration $currentIteration) ---"
          scripts\build.ps1 -Config $env:CONFIG -Arch $env:ARCH -Tls $env:TLS -CI
          scripts\test.ps1 -Filter $env:TEST_FILTER -Config $env:CONFIG -Arch $env:ARCH -Tls $env:TLS -CodeCoverage -GHA -GenerateXmlResults -LogProfile Full.Light

          $summary = Get-ChangedFilesCoverageSummary -XmlPath $coverageXml -ChangedFiles $changedFiles
          $lastSummary = $summary
          Write-Host "Changed-files coverage: $($summary.AggregatePercent)% ($($summary.Covered)/$($summary.Total) covered)"
          $below = @($summary.PerFile | Where-Object { $_.Percent -lt $target })
          if (-not $below -or $below.Count -eq 0) {
            Write-Host "Coverage target reached for changed files (>= $target%)."
            break
          }

          if ($currentIteration -ge $maxIterations) {
            Write-Host "Reached max iterations ($maxIterations). Stopping."
            break
          }

          $bullets = ($changedFiles | ForEach-Object { "- $_" }) -join "`n"
          $coverageLines = ($summary.PerFile | Sort-Object Percent | ForEach-Object {
            $flag = if ($_.Found) { '' } else { ' (not found in report)' }
            "- $($_.File): $($_.Percent)%$flag"
          }) -join "`n"

          $prompt = @(
            $basePrompt.TrimEnd(),
            '',
            "Repository: $($env:GITHUB_REPOSITORY)",
            "PR: #$($env:PR_NUMBER)",
            "Workflow run: $($env:RUN_ID)",
            "Agent: $($env:AGENT_NAME)",
            "Iteration: $currentIteration / $maxIterations",
            "Coverage target: $target% (changed files)",
            '',
            'Changed files:',
            $bullets,
            '',
            'Current changed-files coverage summary:',
            $coverageLines,
            '',
            "Coverage report: $($env:COVERAGE_XML)",
            '',
            'Now update/add tests to improve coverage for the files below target. Prefer src/test/ changes. When you are done, create a PR using the workflow safe output.'
          ) -join "`n"

          Write-Host "--- Invoking agent ($($env:AGENT_NAME)) ---"
          gh copilot --agent $env:AGENT_NAME --allow-all-tools -p "$prompt"

          $status = git status --porcelain
          if ([string]::IsNullOrWhiteSpace($status)) {
            Write-Host 'No changes detected after agent; stopping.'
            break
          }

          $currentIteration++
        }

        if ($lastSummary) {
          "FINAL_CHANGED_FILES_COVERAGE=$($lastSummary.AggregatePercent)" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
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
