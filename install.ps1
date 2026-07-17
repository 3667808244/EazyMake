<#
.SYNOPSIS
    EazyMake Windows installer — installs the prebuilt ezmk.exe from GitHub Releases.

.DESCRIPTION
    Downloads the prebuilt ezmk.exe from GitHub Releases, verifies SHA-256 integrity,
    installs to a user-local directory, configures PATH, and pre-registers the official
    package repository.

    Default install path: $env:LOCALAPPDATA\ezmk (no admin required).

.PARAMETER Version
    Version tag to install (e.g. "0.9.5"). Default: "latest" (auto-detect newest release).

.PARAMETER InstallDir
    Root install directory. The binary goes to <InstallDir>\bin\ezmk.exe.
    Default: $env:LOCALAPPDATA\ezmk

.PARAMETER NoPath
    Skip adding the bin directory to the user PATH environment variable.

.PARAMETER DryRun
    Print what would be done without making any changes.

.EXAMPLE
    # Review the script, then run:
    .\install.ps1

.EXAMPLE
    # One-line remote execution (review recommended first):
    irm https://raw.githubusercontent.com/3667808244/EazyMake/main/install.ps1 | iex

.EXAMPLE
    # Install a specific version to a custom directory:
    .\install.ps1 -Version "0.9.5" -InstallDir "D:\tools\ezmk"

.EXAMPLE
    # Dry run to preview actions:
    .\install.ps1 -DryRun

.NOTES
    Repo: https://github.com/3667808244/EazyMake
    Docs: https://github.com/3667808244/EazyMake#readme
#>

param(
    [string]$Version = "latest",
    [string]$InstallDir = "",
    [switch]$NoPath = $false,
    [switch]$DryRun = $false
)

# ── Strict mode ────────────────────────────────────────────────────────────────
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"  # faster downloads

# ── Constants ──────────────────────────────────────────────────────────────────
$Script:RepoOwner  = "3667808244"
$Script:RepoName   = "EazyMake"
$Script:ApiBase    = "https://api.github.com/repos/$RepoOwner/$RepoName"
$Script:DownloadBase = "https://github.com/$RepoOwner/$RepoName/releases/download"
$Script:OfficialRepo = "https://github.com/$RepoOwner/ezmk-repo.git"
$Script:OfficialRepoName = "official"
$Script:BinName    = "ezmk.exe"

# ── Logging helpers ────────────────────────────────────────────────────────────
function Write-Info {
    param([string]$Message)
    Write-Host "==> " -NoNewline -ForegroundColor Cyan
    Write-Host $Message
}

function Write-Warn {
    param([string]$Message)
    Write-Host "warning: " -NoNewline -ForegroundColor Yellow
    Write-Host $Message
}

function Write-Err {
    param([string]$Message)
    Write-Host "error: " -NoNewline -ForegroundColor Red
    Write-Host $Message
}

function Write-Die {
    param([string]$Message)
    Write-Err $Message
    exit 1
}

# ── Helper functions ───────────────────────────────────────────────────────────

function Test-DryRun {
    return $DryRun.IsPresent
}

function Invoke-RealOrDry {
    param(
        [string]$Description,
        [ScriptBlock]$Action
    )
    Write-Info $Description
    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would execute: $($Action.ToString().Trim())" -ForegroundColor DarkGray
    } else {
        & $Action
    }
}

function Test-CommandExists {
    param([string]$Name)
    return (Get-Command $Name -ErrorAction SilentlyContinue) -ne $null
}

function Get-DefaultInstallDir {
    if ($InstallDir) { return $InstallDir }
    return Join-Path $env:LOCALAPPDATA "ezmk"
}

function Get-BinDir {
    return Join-Path (Get-DefaultInstallDir) "bin"
}

function Get-BinPath {
    return Join-Path (Get-BinDir) $Script:BinName
}

# ── Pre-flight checks ──────────────────────────────────────────────────────────

function Test-Prerequisites {
    Write-Info "Checking environment..."

    # PowerShell version (needs 5.1+ for TLS 1.2 and Expand-Archive)
    if ($PSVersionTable.PSVersion.Major -lt 5) {
        Write-Die "PowerShell 5.1 or later is required. Current: $($PSVersionTable.PSVersion)"
    }

    # Enable TLS 1.2
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Write-Info "PowerShell $($PSVersionTable.PSVersion) — OK"
}

# ── Version resolution ─────────────────────────────────────────────────────────

function Resolve-Version {
    param([string]$RequestedVersion)

    if ($RequestedVersion -eq "latest") {
        Write-Info "Resolving latest version from GitHub Releases..."
        if (Test-DryRun) {
            Write-Host "       [DRY RUN] Would query: $Script:ApiBase/releases/latest" -ForegroundColor DarkGray
            return "0.0.0-dryrun"
        }
        try {
            $release = Invoke-RestMethod -Uri "$Script:ApiBase/releases/latest" -Method Get `
                -Headers @{ "Accept" = "application/vnd.github.v3+json" }
            $tag = $release.tag_name
            Write-Info "Latest version: $tag"
            return $tag
        } catch {
            Write-Die "Failed to query GitHub Releases API. Check your network connection.`n  $($_.Exception.Message)"
        }
    }

    # Validate that the specified version exists
    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would verify version: $RequestedVersion" -ForegroundColor DarkGray
        return $RequestedVersion
    }
    try {
        $release = Invoke-RestMethod -Uri "$Script:ApiBase/releases/tags/$RequestedVersion" -Method Get `
            -Headers @{ "Accept" = "application/vnd.github.v3+json" }
        Write-Info "Found release: $($release.tag_name)"
        return $release.tag_name
    } catch {
        Write-Die "Version '$RequestedVersion' not found on GitHub Releases. Check the tag name.`n  $($_.Exception.Message)"
    }
}

# ── Download ───────────────────────────────────────────────────────────────────

function Invoke-Download {
    param(
        [string]$VersionTag,
        [string]$DestDir
    )

    $url = "$Script:DownloadBase/$VersionTag/$Script:BinName"
    $dest = Join-Path $DestDir $Script:BinName
    $checksumUrl = "$Script:DownloadBase/$VersionTag/$Script:BinName.sha256"
    $checksumDest = Join-Path $DestDir "$Script:BinName.sha256"

    Write-Info "Downloading ezmk.exe (version $VersionTag)..."
    Write-Info "  URL: $url"

    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would download to: $dest" -ForegroundColor DarkGray
        return @{ Binary = $dest; Checksum = $checksumDest }
    }

    # Download binary
    try {
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
    } catch {
        Write-Die "Failed to download ezmk.exe. Check your network and the version tag.`n  $($_.Exception.Message)"
    }

    if (-not (Test-Path $dest)) {
        Write-Die "Download completed but file not found: $dest"
    }

    $size = (Get-Item $dest).Length
    Write-Info "Downloaded: $dest ($([math]::Round($size/1KB, 1)) KB)"

    # Try to download checksum file (non-fatal if missing)
    try {
        Invoke-WebRequest -Uri $checksumUrl -OutFile $checksumDest -UseBasicParsing
        Write-Info "Downloaded checksum: $checksumDest"
    } catch {
        Write-Warn "SHA-256 checksum file not available for this release (downloaded without verification)."
        Remove-Item $checksumDest -ErrorAction SilentlyContinue
        $checksumDest = $null
    }

    return @{ Binary = $dest; Checksum = $checksumDest }
}

# ── SHA-256 verification ──────────────────────────────────────────────────────

function Test-Checksum {
    param(
        [string]$BinaryPath,
        [string]$ChecksumPath
    )

    if (-not $ChecksumPath -or -not (Test-Path $ChecksumPath)) {
        Write-Warn "No checksum file — skipping verification."
        return $true
    }

    Write-Info "Verifying SHA-256 checksum..."

    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would verify: $BinaryPath" -ForegroundColor DarkGray
        return $true
    }

    $actualHash = (Get-FileHash -Path $BinaryPath -Algorithm SHA256).Hash.ToLower()
    $expectedLine = Get-Content $ChecksumPath -First 1
    # Expected format: "<sha256>  ezmk.exe" or just "<sha256>"
    $expectedHash = ($expectedLine -split '\s+')[0].ToLower()

    if ($actualHash -ne $expectedHash) {
        Write-Err "SHA-256 mismatch!"
        Write-Err "  Expected: $expectedHash"
        Write-Err "  Actual:   $actualHash"
        Write-Die "Checksum verification failed. The downloaded file may be corrupted or tampered with."
    }

    Write-Info "SHA-256 checksum OK"
    return $true
}

# ── Install ────────────────────────────────────────────────────────────────────

function Install-Ezmk {
    param([string]$SourceBinary)

    $binDir = Get-BinDir
    $dest = Get-BinPath

    Write-Info "Installing to: $dest"

    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would install: $SourceBinary -> $dest" -ForegroundColor DarkGray
        return
    }

    # Create target directory
    if (-not (Test-Path $binDir)) {
        New-Item -ItemType Directory -Path $binDir -Force | Out-Null
    }

    # Atomic install: copy to temp name, then rename into place
    $tmpDest = Join-Path $binDir ".ezmk.install.$pid"
    Copy-Item $SourceBinary $tmpDest -Force

    # Remove existing if present
    if (Test-Path $dest) {
        Remove-Item $dest -Force
    }

    Move-Item $tmpDest $dest -Force
    Write-Info "Installed: $dest"
}

# ── PATH configuration ─────────────────────────────────────────────────────────

function Update-Path {
    if ($NoPath.IsPresent) {
        Write-Info "-NoPath specified — skipping PATH configuration."
        return
    }

    $binDir = Get-BinDir
    Write-Info "Configuring user PATH..."

    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would add to user PATH: $binDir" -ForegroundColor DarkGray
        return
    }

    # Read current user PATH
    $currentUserPath = [Environment]::GetEnvironmentVariable("Path", "User") -split ';' |
        Where-Object { $_ -ne '' }

    # Check if already present (case-insensitive comparison on Windows)
    $alreadyPresent = $currentUserPath | Where-Object {
        (Resolve-Path $_ -ErrorAction SilentlyContinue).ProviderPath.TrimEnd('\') -eq
        (Resolve-Path $binDir -ErrorAction SilentlyContinue).ProviderPath.TrimEnd('\')
    }

    if ($alreadyPresent) {
        Write-Info "$binDir is already in your user PATH — skipping."
        return
    }

    # Also check bare string match (for non-resolvable paths)
    if ($currentUserPath -contains $binDir) {
        Write-Info "$binDir is already in your user PATH — skipping."
        return
    }

    # Append to user PATH
    $newPath = ($currentUserPath + $binDir) -join ';'
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")

    Write-Info "Added to user PATH: $binDir"
    Write-Warn "You may need to restart your terminal for PATH changes to take effect."
}

# ── Official repo pre-registration ─────────────────────────────────────────────

function Register-OfficialRepo {
    $ezmkBin = Get-BinPath

    if (-not (Test-Path $ezmkBin)) {
        Write-Warn "ezmk.exe not found at $ezmkBin — skipping repo registration."
        return
    }

    Write-Info "Registering official package repository ($Script:OfficialRepoName)..."

    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would run: $ezmkBin repo add -u $Script:OfficialRepo --name $Script:OfficialRepoName" -ForegroundColor DarkGray
        Write-Host "       [DRY RUN] Would run: $ezmkBin repo update -u $Script:OfficialRepoName" -ForegroundColor DarkGray
        return
    }

    # repo add -u (user scope)
    $addResult = & $ezmkBin repo add -u $Script:OfficialRepo --name $Script:OfficialRepoName 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Info "Registered official repo ($Script:OfficialRepoName) to user scope"
    } else {
        Write-Warn "Could not register official repo (it may already exist or network is unavailable)."
        Write-Warn "Run 'ezmk repo add -u $Script:OfficialRepo --name $Script:OfficialRepoName' manually later."
        return
    }

    # repo update -u (fetch index)
    $updateResult = & $ezmkBin repo update -u $Script:OfficialRepoName 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Info "Updated official repo index"
    } else {
        Write-Warn "Could not update official repo (no network?); run 'ezmk repo update -u $Script:OfficialRepoName' later."
    }
}

# ── Verification ───────────────────────────────────────────────────────────────

function Confirm-Installation {
    $ezmkBin = Get-BinPath

    if (-not (Test-Path $ezmkBin)) {
        Write-Warn "Cannot verify — ezmk.exe not found at $ezmkBin"
        return
    }

    Write-Info ""

    if (Test-DryRun) {
        Write-Host "       [DRY RUN] Would run: $ezmkBin version" -ForegroundColor DarkGray
        return
    }

    Write-Info "Verifying installation..."
    Write-Host "────────────────────────────────────────────────────────────"

    $versionOutput = & $ezmkBin version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host $versionOutput
    } else {
        Write-Warn "ezmk --version returned non-zero exit code. Installation may be incomplete."
    }

    Write-Host ""
    Write-Info "Done. EazyMake is installed at: $ezmkBin"
}

# ── PATH reminder ──────────────────────────────────────────────────────────────

function Show-PathReminder {
    if ($NoPath.IsPresent) { return }

    $binDir = Get-BinDir
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User") -split ';'
    $inPath = ($currentPath -contains $binDir)

    if (-not $inPath -and -not (Test-DryRun)) {
        Write-Host ""
        Write-Warn "$binDir is not in your PATH. Add it manually:"
        Write-Host "  setx PATH `"%PATH%;$binDir`""
        Write-Host "  (restart your terminal afterward)"
    }
}

# ── Cleanup ────────────────────────────────────────────────────────────────────

function Remove-TempFiles {
    param([string]$TempDir)
    if ($TempDir -and (Test-Path $TempDir)) {
        Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════════

function Main {
    Write-Host ""
    Write-Host "  ______                __  ___            " -ForegroundColor Cyan
    Write-Host " /_  __/__ ___ _  __ _/  |/  /___ _____   " -ForegroundColor Cyan
    Write-Host "  / / /_  __  / |/_/ / /|_/ / _  / __ \  " -ForegroundColor Cyan
    Write-Host " /_/ /_/ /_/ /_>  </ /_/ /_/  \_,_/_/ /_/  " -ForegroundColor Cyan
    Write-Host "                                          " -ForegroundColor Cyan
    Write-Host "  EazyMake Windows Installer" -ForegroundColor White
    Write-Host ""

    if (Test-DryRun) {
        Write-Warn "DRY RUN MODE — no changes will be made."
        Write-Host ""
    }

    # 1. Pre-flight checks
    Test-Prerequisites

    # 2. Resolve version
    $resolvedVersion = Resolve-Version -RequestedVersion $Version

    # 3. Prepare temp directory
    $tempDir = ""
    if (-not (Test-DryRun)) {
        $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "ezmk-install.$pid"
        New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
    }

    try {
        # 4. Download
        $downloadResult = Invoke-Download -VersionTag $resolvedVersion -DestDir $tempDir

        # 5. SHA-256 verification
        Test-Checksum -BinaryPath $downloadResult.Binary -ChecksumPath $downloadResult.Checksum

        # 6. Install
        Install-Ezmk -SourceBinary $downloadResult.Binary

        # 7. PATH configuration
        Update-Path

        # 8. Official repo pre-registration
        Register-OfficialRepo

        # 9. Verification
        Confirm-Installation

        # 10. PATH reminder
        Show-PathReminder

    } finally {
        # Cleanup temp files
        Remove-TempFiles $tempDir
    }

    Write-Host ""
}

# ── Entry point ────────────────────────────────────────────────────────────────
Main
