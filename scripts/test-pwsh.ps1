Write-Host "PowerShell Version Test"
Write-Host "========================"
$PSVersionTable | Format-Table -AutoSize
Write-Host "pwsh path: $(Get-Command pwsh | Select-Object -ExpandProperty Source)"
