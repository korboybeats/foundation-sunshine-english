<#
.SYNOPSIS
    Foundation Sunshine - English Edition launcher.

.DESCRIPTION
    Start Menu / Desktop shortcut target. Ensures SunshineService is running
    (auto-elevating via UAC if needed), then opens the Web UI in the default
    browser.

    Behavior:
      - Service already running:  no UAC prompt, just opens browser (fast path).
      - Service stopped:          UAC prompt, starts service, opens browser.
      - Service missing:          opens browser anyway (will fail to load,
                                  user can reinstall).

    Logs to: %LOCALAPPDATA%\SunshineEnglishEdition\launcher.log

.NOTES
    Deployed by Inno Setup to: <InstallDir>\scripts\sunshine-launcher.ps1
#>

$ErrorActionPreference = 'SilentlyContinue'

# Logging — paths with spaces have bitten this script before, keep an
# audit trail of what each invocation actually did.
$LogPath = Join-Path $env:LOCALAPPDATA 'SunshineEnglishEdition\launcher.log'
$logDir = Split-Path -Parent $LogPath
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
}
function Log([string]$msg) {
    try {
        Add-Content -Path $LogPath -Value "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] $msg" -Encoding UTF8
    } catch {}
}

# Wait for the Sunshine Web UI port to actually accept connections. The
# SCM reports the service "Running" the moment its main thread starts,
# but Sunshine still needs ~1-3s to bind its HTTPS listener on 47990.
# Returns $true if the port becomes reachable within $TimeoutSec.
function Wait-ForWebUI([int]$TimeoutSec = 15) {
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $tcp = $null
        try {
            $tcp = New-Object System.Net.Sockets.TcpClient
            $task = $tcp.ConnectAsync('127.0.0.1', 47990)
            if ($task.Wait(500) -and $tcp.Connected) {
                return $true
            }
        } catch {} finally {
            if ($tcp) { $tcp.Close() }
        }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

Log "=== Launcher invoked (PID $PID) ==="
Log "ScriptPath: $PSCommandPath"

$svc = Get-Service -Name 'SunshineService' -ErrorAction SilentlyContinue
if (-not $svc) {
    Log "SunshineService not found; opening browser anyway."
    Start-Process 'https://localhost:47990'
    return
}

Log "Service status: $($svc.Status); startType: $($svc.StartType)"

if ($svc.Status -eq 'Running') {
    Log "Service already running. Opening browser."
    Start-Process 'https://localhost:47990'
    return
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
Log "isAdmin: $isAdmin"

if (-not $isAdmin) {
    # Re-launch this script elevated. Build the argument as a single string
    # with the script path quoted, so spaces in "C:\Program Files\..." do
    # not break the -File parameter on the elevated side.
    $argString = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$PSCommandPath`""
    Log "Elevating: powershell.exe $argString"
    try {
        Start-Process -FilePath 'powershell.exe' -Verb RunAs -WindowStyle Hidden -Wait -ArgumentList $argString
        Log "Elevated process exited."
    } catch {
        Log "ERROR elevating: $($_.Exception.Message)"
    }
} else {
    try {
        if ("$($svc.StartType)" -eq 'Disabled') {
            Set-Service -Name 'SunshineService' -StartupType Automatic
            Log "Restored startup type to Automatic."
        }
        Start-Service -Name 'SunshineService' -ErrorAction Stop
        Log "Service started."
    } catch {
        Log "ERROR starting service: $($_.Exception.Message)"
    }
}

# Re-check status after the elevated branch returns. Only opens the browser
# from the user-context (non-elevated) invocation; the elevated child returns
# above without reaching here a second time.
if (-not $isAdmin) {
    $svc2 = Get-Service -Name 'SunshineService' -ErrorAction SilentlyContinue
    Log "Post-elevation status: $($svc2.Status)"
    if ($svc2 -and $svc2.Status -eq 'Running') {
        $ready = Wait-ForWebUI -TimeoutSec 15
        Log "Web UI ready: $ready"
    }
    Start-Process 'https://localhost:47990'
    Log "Opened browser."
}
