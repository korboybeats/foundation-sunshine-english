<#
.SYNOPSIS
    Foundation Sunshine - English Edition auto-updater.

.DESCRIPTION
    Runs as a scheduled task. Compares the installed wrapper version (read
    from HKLM:\SOFTWARE\SunshineEnglishEdition\Version) against the latest
    release on korboybeats/foundation-sunshine-english. If newer, downloads
    the wrapper installer and runs it /SILENT.

    Idempotent: exits cleanly if already up to date or if not installed.

.NOTES
    Deployed by install.ps1 to: <InstallDir>\scripts\sunshine-english-updater.ps1
    Scheduled task: SunshineEnglishEdition_AutoUpdate (weekly Sunday 3am, SYSTEM)
    Log: %LOCALAPPDATA%\SunshineEnglishEdition\auto-update.log
#>

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# When run as SYSTEM via Task Scheduler, %LOCALAPPDATA% points to
# C:\Windows\System32\config\systemprofile\AppData\Local. That's fine for logs.
$LogPath = Join-Path $env:LOCALAPPDATA "SunshineEnglishEdition\auto-update.log"
$LogDir = Split-Path -Parent $LogPath
if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

function Write-Log([string]$Message) {
    $line = "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] $Message"
    try { Add-Content -Path $LogPath -Value $line -Encoding UTF8 } catch {}
    Write-Host $line
}

Write-Log "=== Auto-update check start ==="

# Read installed version
try {
    $installed = (Get-ItemProperty -Path "HKLM:\SOFTWARE\SunshineEnglishEdition" -ErrorAction Stop).Version
    Write-Log "Installed version: $installed"
} catch {
    Write-Log "ERROR: not installed via Foundation Sunshine - English Edition wrapper. Exiting."
    exit 0
}

# Query latest release
$headers = @{ "User-Agent" = "SunshineEnglishEdition-AutoUpdate" }
try {
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/korboybeats/foundation-sunshine-english/releases/latest" -Headers $headers -TimeoutSec 30
} catch {
    Write-Log "ERROR: failed to query latest release: $($_.Exception.Message)"
    exit 0
}

$latest = $release.tag_name
Write-Log "Latest wrapper release: $latest"

# Always re-run the wrapper, even if wrapper version hasn't changed. The
# wrapper internally downloads the LATEST upstream portable zip on every
# run (auto-current with upstream), so re-running picks up new upstream
# releases even when our wrapper hasn't been re-cut. Wrapper install logic
# is idempotent and fast (~30s when nothing has changed).
if ($latest -eq $installed) {
    Write-Log "Wrapper version unchanged, but re-running anyway to pull latest upstream Sunshine release."
} else {
    Write-Log "New wrapper version available; updating from $installed to $latest."
}

# Find wrapper installer asset
$asset = $release.assets | Where-Object { $_.name -eq "Sunshine-EnglishEdition-Setup.exe" } | Select-Object -First 1
if (-not $asset) {
    Write-Log "ERROR: no Sunshine-EnglishEdition-Setup.exe asset in release $latest. Exiting."
    exit 0
}

$tmp = Join-Path $env:TEMP "Sunshine-EnglishEdition-Setup-$latest.exe"
Write-Log "Downloading $($asset.name) ($([math]::Round($asset.size / 1MB, 1)) MB) ..."
try {
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tmp -Headers $headers -TimeoutSec 600
} catch {
    Write-Log "ERROR: download failed: $($_.Exception.Message)"
    exit 0
}

# Preserve the user's update-channel choice across silent re-installs.
# Inno wizard tasks default to unchecked under /SILENT, so without /TASKS
# the prereleases task would silently flip back to off on every weekly run.
$wrapperArgs = @("/SILENT", "/SUPPRESSMSGBOXES", "/NORESTART")
try {
    $tp = (Get-ItemProperty -Path "HKLM:\SOFTWARE\SunshineEnglishEdition" -Name "TrackPrereleases" -ErrorAction Stop).TrackPrereleases
    if ($tp -eq 1) {
        $wrapperArgs += "/TASKS=prereleases"
        Write-Log "Preserving TrackPrereleases=1 across re-install."
    }
} catch {}

Write-Log "Running wrapper installer: $($wrapperArgs -join ' ')"
try {
    $proc = Start-Process -FilePath $tmp -ArgumentList $wrapperArgs -Wait -PassThru -NoNewWindow
    Write-Log "Wrapper exit code: $($proc.ExitCode)"
} catch {
    Write-Log "ERROR: wrapper launch failed: $($_.Exception.Message)"
}

Remove-Item -Path $tmp -ErrorAction SilentlyContinue

Write-Log "=== Auto-update check end ==="
