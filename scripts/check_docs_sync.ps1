# check_docs_sync.ps1 — Verify docs/ and tutorial/ en↔zh file correspondence
# Exit 1 if any mismatch is found.

$ErrorActionPreference = "Stop"
$failures = 0

function Check-Dir {
    param([string]$Dir)

    $enDir = Join-Path $Dir "en"
    $zhDir = Join-Path $Dir "zh"

    if (-not (Test-Path $enDir)) {
        Write-Host "MISSING: $enDir" -ForegroundColor Red
        $script:failures++
        return
    }
    if (-not (Test-Path $zhDir)) {
        Write-Host "MISSING: $zhDir" -ForegroundColor Red
        $script:failures++
        return
    }

    $enFiles = (Get-ChildItem -Path $enDir -Filter "*.md" -File | ForEach-Object { $_.Name }) | Sort-Object
    $zhFiles = (Get-ChildItem -Path $zhDir -Filter "*.md" -File | ForEach-Object { $_.Name }) | Sort-Object

    $diff = Compare-Object $enFiles $zhFiles
    if ($diff) {
        Write-Host "MISMATCH in $Dir :" -ForegroundColor Red
        $diff | ForEach-Object {
            if ($_.SideIndicator -eq "<=") {
                Write-Host "  en/ only: $($_.InputObject)" -ForegroundColor Yellow
            } else {
                Write-Host "  zh/ only: $($_.InputObject)" -ForegroundColor Yellow
            }
        }
        $script:failures++
    } else {
        Write-Host "OK: $Dir ($($enFiles.Count) files matched)" -ForegroundColor Green
    }
}

Write-Host "=== EazyMake docs sync check ==="
Write-Host ""

Push-Location (Split-Path -Parent (Split-Path -Parent $PSCommandPath))
Check-Dir "docs"
Check-Dir "tutorial"
Pop-Location

Write-Host ""
if ($failures -eq 0) {
    Write-Host "All checks passed." -ForegroundColor Green
    exit 0
} else {
    Write-Host "$failures check(s) failed." -ForegroundColor Red
    exit 1
}
