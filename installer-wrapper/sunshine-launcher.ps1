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

.NOTES
    Deployed by Inno Setup to: <InstallDir>\scripts\sunshine-launcher.ps1
    Shortcut: powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File <this>
#>

$ErrorActionPreference = 'SilentlyContinue'

$svc = Get-Service -Name 'SunshineService'
if ($svc -and $svc.Status -ne 'Running') {
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        # Re-launch this script elevated, then continue once it returns.
        try {
            Start-Process powershell -Verb RunAs -WindowStyle Hidden -Wait -ArgumentList @(
                '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', $PSCommandPath
            )
        } catch {
            # User declined UAC — fall through and try to open the UI anyway.
        }
    } else {
        try {
            # Restore startup type if disabled, then start.
            $current = (Get-Service -Name 'SunshineService').StartType
            if ("$current" -eq 'Disabled') {
                Set-Service -Name 'SunshineService' -StartupType Automatic
            }
            Start-Service -Name 'SunshineService'
        } catch {}
    }
}

Start-Process 'https://localhost:47990'
