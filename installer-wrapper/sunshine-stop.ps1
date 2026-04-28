<#
.SYNOPSIS
    Stop the Foundation Sunshine - English Edition service.

.DESCRIPTION
    Start Menu shortcut target. Stops SunshineService, auto-elevating via
    UAC if needed. Logs to %LOCALAPPDATA%\SunshineEnglishEdition\launcher.log.

.NOTES
    Deployed by Inno Setup to: <InstallDir>\scripts\sunshine-stop.ps1
#>

$ErrorActionPreference = 'SilentlyContinue'

$LogPath = Join-Path $env:LOCALAPPDATA 'SunshineEnglishEdition\launcher.log'
$logDir = Split-Path -Parent $LogPath
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
}
function Log([string]$msg) {
    try {
        Add-Content -Path $LogPath -Value "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] [stop] $msg" -Encoding UTF8
    } catch {}
}

Log "=== Stop invoked (PID $PID) ==="

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
Log "isAdmin: $isAdmin"

if (-not $isAdmin) {
    # Quote the script path: default install dir "C:\Program Files\Sunshine"
    # contains a space that would otherwise truncate the -File argument.
    $argString = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$PSCommandPath`""
    Log "Elevating: powershell.exe $argString"
    try {
        Start-Process -FilePath 'powershell.exe' -Verb RunAs -WindowStyle Hidden -ArgumentList $argString
    } catch {
        Log "ERROR elevating: $($_.Exception.Message)"
    }
    return
}

try {
    Stop-Service -Name 'SunshineService' -Force -ErrorAction Stop
    Log "Service stopped."
} catch {
    Log "ERROR stopping service: $($_.Exception.Message)"
}
