<#
.SYNOPSIS
    Stop the Foundation Sunshine - English Edition service.

.DESCRIPTION
    Start Menu shortcut target. Stops SunshineService, auto-elevating via
    UAC if needed.

.NOTES
    Deployed by Inno Setup to: <InstallDir>\scripts\sunshine-stop.ps1
#>

$ErrorActionPreference = 'SilentlyContinue'

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    try {
        Start-Process powershell -Verb RunAs -WindowStyle Hidden -ArgumentList @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass',
            '-File', $PSCommandPath
        )
    } catch {}
    exit
}

try {
    Stop-Service -Name 'SunshineService' -Force
} catch {}
