<#
.SYNOPSIS
    Foundation Sunshine - English Edition install logic.

.DESCRIPTION
    Invoked by the Inno Setup wrapper after its own [Files] section has
    extracted the English overlay into a staging directory. This script:
      1. Downloads the latest AlkaidLab/foundation-sunshine installer
      2. Runs it silently with the user-selected components
      3. Copies the English overlay over the install dir
      4. Optionally registers the vmouse driver

    Wrapper passes paths/options via parameters so all logic stays here
    where it can be tested and iterated on without recompiling Inno.

.PARAMETER InstallDir
    Where Sunshine should be installed (e.g. "C:\Program Files\Sunshine").

.PARAMETER OverlayDir
    Path to the directory containing the staged English overlay
    (sunshine.exe, assets\web\, scripts\vmouse\*.bat).

.PARAMETER Components
    Comma-separated list of upstream components to install
    (e.g. "application,assets,vdd,vmouse,tools").

.PARAMETER InstallVmouse
    If present, runs install-vmouse.bat post-install.

.PARAMETER LogPath
    Where to write the install log.

.EXAMPLE
    pwsh install.ps1 -InstallDir "C:\Program Files\Sunshine" `
        -OverlayDir "C:\Users\foo\AppData\Local\Temp\overlay" `
        -Components "application,assets,vdd,vmouse,tools" `
        -InstallVmouse `
        -LogPath "C:\Users\foo\AppData\Local\Temp\install.log"
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$InstallDir,
    [Parameter(Mandatory)] [string]$OverlayDir,
    [Parameter(Mandatory)] [string]$Components,
    [switch]$InstallVmouse,
    [string]$LogPath,
    [string]$WrapperVersion = "unknown",
    [string]$WrapperSourceDir
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $LogPath) {
    $LogPath = Join-Path $env:LOCALAPPDATA "SunshineEnglishEdition\install.log"
}

# Ensure the log's parent directory exists before any logging
$logDir = Split-Path -Parent $LogPath
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
}

# Truncate any prior log so we always see the latest run only
"" | Set-Content -Path $LogPath -Encoding UTF8

function Write-Log([string]$Message) {
    $timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    $line = "[$timestamp] $Message"
    Write-Host $line
    try {
        Add-Content -Path $LogPath -Value $line -Encoding UTF8 -ErrorAction Stop
    } catch {
        # Logging failure is non-fatal; the script continues
        Write-Host "WARN: log write failed: $($_.Exception.Message)"
    }
}

function Abort-Install([string]$Message) {
    Write-Log "ERROR: $Message"
    throw $Message
}

# ---------------------------------------------------------------------------
# 1. Download upstream installer
# ---------------------------------------------------------------------------
function Get-UpstreamInstaller {
    Write-Log "Querying upstream release..."

    $headers = @{
        "User-Agent" = "SunshineEnglishEdition-Installer"
        "Accept"     = "application/vnd.github+json"
    }

    try {
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/AlkaidLab/foundation-sunshine/releases/latest" -Headers $headers -TimeoutSec 30
    } catch {
        Abort-Install "Failed to query upstream release: $($_.Exception.Message)"
    }

    Write-Log "Upstream tag: $($release.tag_name)"

    $asset = $release.assets | Where-Object { $_.name -like "*WindowsInstaller.exe" } | Select-Object -First 1
    if (-not $asset) {
        Abort-Install "No *WindowsInstaller.exe asset in upstream release $($release.tag_name)"
    }

    Write-Log "Found asset: $($asset.name) ($([math]::Round($asset.size / 1MB, 1)) MB)"

    $cacheDir = Join-Path $env:LOCALAPPDATA "SunshineEnglishEdition\cache"
    if (-not (Test-Path $cacheDir)) {
        New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
    }
    $cachePath = Join-Path $cacheDir $asset.name

    if (Test-Path $cachePath) {
        Write-Log "Using cached installer: $cachePath"
        return $cachePath
    }

    Write-Log "Downloading from $($asset.browser_download_url) ..."
    $maxAttempts = 3
    for ($i = 1; $i -le $maxAttempts; $i++) {
        try {
            Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $cachePath -Headers $headers -TimeoutSec 600
            Write-Log "Download succeeded ($([math]::Round((Get-Item $cachePath).Length / 1MB, 1)) MB)"
            return $cachePath
        } catch {
            Write-Log "Attempt $i/$maxAttempts failed: $($_.Exception.Message)"
            if ($i -lt $maxAttempts) {
                $backoff = 5 * $i * $i
                Write-Log "Retrying in $backoff seconds..."
                Start-Sleep -Seconds $backoff
            }
        }
    }

    Abort-Install "Failed to download upstream installer after $maxAttempts attempts."
}

# ---------------------------------------------------------------------------
# 2. Run upstream installer
# ---------------------------------------------------------------------------
function Invoke-UpstreamInstaller([string]$InstallerPath) {
    $upstreamLog = Join-Path $env:TEMP "upstream-install.log"

    $args = @(
        "/VERYSILENT",
        "/SUPPRESSMSGBOXES",
        "/NORESTART",
        "/DIR=`"$InstallDir`"",
        "/COMPONENTS=`"$Components`"",
        "/LOG=`"$upstreamLog`""
    )

    Write-Log "Running upstream installer:"
    Write-Log "  $InstallerPath $($args -join ' ')"

    $proc = Start-Process -FilePath $InstallerPath -ArgumentList $args -Wait -PassThru -NoNewWindow
    Write-Log "Upstream installer exit code: $($proc.ExitCode)"

    # 0 = success, 3010 = success-needs-reboot
    if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 3010) {
        Write-Log "Upstream installer log: $upstreamLog"
        Abort-Install "Upstream installer failed with exit code $($proc.ExitCode). See log: $upstreamLog"
    }
}

# ---------------------------------------------------------------------------
# 3. Apply English overlay
# ---------------------------------------------------------------------------
function Test-FileLocked([string]$Path) {
    if (-not (Test-Path $Path)) { return $false }
    try {
        $stream = [System.IO.File]::Open($Path, 'Open', 'ReadWrite', 'None')
        $stream.Close()
        return $false
    } catch {
        return $true
    }
}

function Stop-SunshineProcesses {
    # Upstream installer auto-starts SunshineService and may launch sunshine.exe
    # or sunshine-gui.exe via [Run]/finish-page. We must stop AND disable the
    # service so it can't restart mid-overlay, then kill any lingering processes.
    Write-Log "Stopping Sunshine service + processes (so we can overlay the binary)..."
    $svc = Get-Service -Name "SunshineService" -ErrorAction SilentlyContinue
    if ($svc) {
        if ($svc.Status -ne 'Stopped') {
            try {
                Stop-Service -Name "SunshineService" -Force -ErrorAction Stop
                Write-Log "  SunshineService stopped."
                $script:RestartSunshineService = $true
            } catch {
                Write-Log "  WARN: failed to stop SunshineService: $($_.Exception.Message)"
            }
        }
        # Disable startup so SCM can't auto-restart mid-copy. We restore at the end.
        try {
            if (-not $script:OriginalServiceStartType) {
                $script:OriginalServiceStartType = $svc.StartType
            }
            Set-Service -Name "SunshineService" -StartupType Disabled -ErrorAction Stop
            Write-Log "  SunshineService startup temporarily disabled (was $($script:OriginalServiceStartType))."
        } catch {
            Write-Log "  WARN: failed to disable SunshineService startup: $($_.Exception.Message)"
        }
    }
    # Use taskkill /F /T to tree-kill (any child processes get killed too).
    # Loop up to 8 passes catching anything that respawns. Verify sunshine.exe
    # is not file-locked before declaring success.
    $sunshineExe = Join-Path $InstallDir "sunshine.exe"
    $sunshineGuiExe = Join-Path $InstallDir "assets\gui\sunshine-gui.exe"
    for ($i = 1; $i -le 8; $i++) {
        # taskkill returns non-zero if process not found, ignore stderr
        $null = & cmd /c "taskkill /F /T /IM sunshine.exe /IM sunshine-gui.exe /IM sunshinesvc.exe /IM qiin-tabtip.exe 2>nul"
        Start-Sleep -Milliseconds 500
        $stillRunning = Get-Process -Name "sunshine", "sunshine-gui", "sunshinesvc", "qiin-tabtip" -ErrorAction SilentlyContinue
        $exeLocked = (Test-FileLocked $sunshineExe) -or (Test-FileLocked $sunshineGuiExe)
        if (-not $stillRunning -and -not $exeLocked) {
            Write-Log "  All processes killed; binaries unlocked (after $i pass(es))."
            return
        }
        if ($stillRunning) {
            Write-Log "  Pass ${i}: still running: $(($stillRunning | ForEach-Object { $_.Name }) -join ', ')"
        }
        if ($exeLocked) {
            Write-Log "  Pass ${i}: sunshine.exe still locked"
        }
    }
    Write-Log "  WARN: gave up after 8 kill passes; copy may still fail with file-in-use."
}

function Restart-SunshineService {
    # Restore service startup type then start it.
    $svc = Get-Service -Name "SunshineService" -ErrorAction SilentlyContinue
    if (-not $svc) { return }
    $restoreType = $script:OriginalServiceStartType
    if (-not $restoreType -or $restoreType -eq 'Disabled') { $restoreType = 'Automatic' }
    try {
        Set-Service -Name "SunshineService" -StartupType $restoreType -ErrorAction Stop
        Write-Log "Restored SunshineService startup type to $restoreType."
    } catch {
        Write-Log "WARN: failed to restore SunshineService startup type: $($_.Exception.Message)"
    }
    if ($script:RestartSunshineService) {
        try {
            Start-Service -Name "SunshineService" -ErrorAction Stop
            Write-Log "SunshineService started."
        } catch {
            Write-Log "WARN: failed to start SunshineService: $($_.Exception.Message). Start manually if needed."
        }
    }
}

function Copy-Overlay {
    if (-not (Test-Path $OverlayDir)) {
        Abort-Install "Overlay directory not found: $OverlayDir"
    }
    if (-not (Test-Path $InstallDir)) {
        Abort-Install "Install directory not found: $InstallDir (upstream install may have failed silently)"
    }

    Stop-SunshineProcesses

    Write-Log "Copying English overlay $OverlayDir -> $InstallDir"
    $count = 0
    Get-ChildItem -Path $OverlayDir -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($OverlayDir.Length).TrimStart('\', '/')
        $dest = Join-Path $InstallDir $relative
        $destDir = Split-Path -Parent $dest
        if (-not (Test-Path $destDir)) {
            New-Item -ItemType Directory -Force -Path $destDir | Out-Null
        }
        # Retry up to 3 times with another kill if the file is locked. This
        # handles the case where a sunshine/sunshine-gui process respawned
        # between Stop-SunshineProcesses and the actual file copy (upstream's
        # finish-page checkbox or user manually launching the GUI).
        $copied = $false
        for ($attempt = 1; $attempt -le 3 -and -not $copied; $attempt++) {
            try {
                Copy-Item -Path $_.FullName -Destination $dest -Force -ErrorAction Stop
                $copied = $true
            } catch {
                Write-Log "  Copy attempt $attempt failed for $relative ($($_.Exception.Message)); re-killing Sunshine and retrying..."
                Stop-SunshineProcesses
            }
        }
        if (-not $copied) {
            Abort-Install "Failed to overlay $relative after 3 attempts. Close all Sunshine windows manually and retry."
        }
        $count++
    }
    Write-Log "Copied $count overlay files."

    # Sanity check
    $marker = Join-Path $InstallDir "sunshine.exe"
    if (-not (Test-Path $marker)) {
        Abort-Install "Overlay verification failed: $marker missing after copy."
    }
    Write-Log "Overlay verification OK."
}

# ---------------------------------------------------------------------------
# 4. Optional: register vmouse driver
# ---------------------------------------------------------------------------
function Install-Vmouse {
    if (-not $InstallVmouse) {
        Write-Log "vmouse register skipped (component not selected)."
        return
    }

    $script = Join-Path $InstallDir "scripts\vmouse\install-vmouse.bat"
    if (-not (Test-Path $script)) {
        Write-Log "WARNING: install-vmouse.bat not found - vmouse component may not have been installed."
        return
    }

    Write-Log "Registering vmouse driver: $script"
    $proc = Start-Process -FilePath $script -WorkingDirectory (Split-Path -Parent $script) -Wait -PassThru -NoNewWindow
    Write-Log "install-vmouse.bat exit code: $($proc.ExitCode)"
    if ($proc.ExitCode -ne 0) {
        Write-Log "WARNING: vmouse register exit code $($proc.ExitCode). Sunshine works; only vmouse feature unavailable. Re-run $script as admin to retry."
    }
}

# ---------------------------------------------------------------------------
# 4b. Install ViGEmBus (virtual gamepad driver)
# ---------------------------------------------------------------------------
function Install-Gamepad {
    # Install ViGEmBus directly from nefarius/ViGEmBus latest release.
    # We bypass upstream's install-gamepad.bat because it downloads via
    # mirror.ghproxy.com (a Chinese GitHub proxy) which fails silently
    # outside China — curl gets an HTML error page, runs an invalid .exe,
    # script returns 0 success, ViGEmBus is never actually installed.
    $vigemSys = Join-Path $env:SystemRoot "System32\drivers\ViGEmBus.sys"
    if (Test-Path $vigemSys) {
        try {
            $existingVer = (Get-Item $vigemSys).VersionInfo.FileVersion
            if ([System.Version]$existingVer -ge [System.Version]"1.17") {
                Write-Log "ViGEmBus already installed (v$existingVer >= 1.17); skipping."
                return
            }
            Write-Log "ViGEmBus v$existingVer is older than 1.17; will reinstall."
        } catch {
            Write-Log "Could not parse existing ViGEmBus version; will reinstall."
        }
    }

    Write-Log "Querying nefarius/ViGEmBus latest release..."
    $headers = @{ "User-Agent" = "SunshineEnglishEdition-Installer" }
    try {
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/nefarius/ViGEmBus/releases/latest" -Headers $headers -TimeoutSec 30
    } catch {
        Write-Log "WARNING: failed to query ViGEmBus release: $($_.Exception.Message). Sunshine works; gamepad input from clients unavailable. Install manually from https://github.com/nefarius/ViGEmBus/releases"
        return
    }

    # Asset naming has varied across ViGEmBus releases:
    #  - older: ViGEmBusSetup_*.exe
    #  - 1.22.0+: ViGEmBus_<version>_x64_x86_arm64.exe (no "Setup" in name)
    #  - some older: ViGEmBus_*.msi
    # Match anything starting with ViGEmBus and ending in .exe or .msi.
    $asset = $release.assets | Where-Object { $_.name -like "ViGEmBus*.exe" -or $_.name -like "ViGEmBus*.msi" } | Select-Object -First 1
    if (-not $asset) {
        Write-Log "WARNING: no installer asset found in ViGEmBus release $($release.tag_name). Install manually from $($release.html_url)"
        return
    }
    Write-Log "Selected asset: $($asset.name)"

    $tmp = Join-Path $env:TEMP "ViGEmBus_$($asset.name)"
    Write-Log "Downloading $($asset.name) ($([math]::Round($asset.size / 1MB, 1)) MB) from GitHub..."
    try {
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tmp -Headers $headers -TimeoutSec 300
    } catch {
        Write-Log "WARNING: ViGEmBus download failed: $($_.Exception.Message). Install manually from $($release.html_url)"
        return
    }

    Write-Log "Running ViGEmBus installer silently..."
    if ($asset.name -like "*.msi") {
        $proc = Start-Process -FilePath "msiexec.exe" -ArgumentList "/i", "`"$tmp`"", "/passive", "/promptrestart" -Wait -PassThru -NoNewWindow
    } else {
        $proc = Start-Process -FilePath $tmp -ArgumentList "/passive", "/promptrestart" -Wait -PassThru -NoNewWindow
    }
    Write-Log "ViGEmBus installer exit code: $($proc.ExitCode)"

    Remove-Item -Path $tmp -ErrorAction SilentlyContinue

    if (Test-Path $vigemSys) {
        $newVer = (Get-Item $vigemSys).VersionInfo.FileVersion
        Write-Log "ViGEmBus.sys present after install (v$newVer)."
    } else {
        Write-Log "WARNING: ViGEmBus.sys not present after installer ran. Sunshine works; gamepad input from clients unavailable. Try running $tmp manually as admin, or install from $($release.html_url)"
    }
}

# ---------------------------------------------------------------------------
# 5. Update wrapper version registry key
# ---------------------------------------------------------------------------
function Set-VersionKey {
    $key = "HKLM:\SOFTWARE\SunshineEnglishEdition"
    if (-not (Test-Path $key)) {
        New-Item -Path $key -Force | Out-Null
    }
    Set-ItemProperty -Path $key -Name "Version" -Value $WrapperVersion
    Set-ItemProperty -Path $key -Name "InstalledAtUtc" -Value (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    Write-Log "Wrote registry key: $key (Version=$WrapperVersion)"
}

# ---------------------------------------------------------------------------
# 6. Set up auto-update scheduled task
# ---------------------------------------------------------------------------
function Setup-AutoUpdate {
    # Deploy auto-update.ps1 to a stable path so the scheduled task can find it.
    if (-not $WrapperSourceDir -or -not (Test-Path (Join-Path $WrapperSourceDir "auto-update.ps1"))) {
        Write-Log "auto-update.ps1 not found in source dir; skipping auto-update setup."
        return
    }
    $updaterDest = Join-Path $InstallDir "scripts\sunshine-english-updater.ps1"
    $updaterDir = Split-Path -Parent $updaterDest
    if (-not (Test-Path $updaterDir)) {
        New-Item -ItemType Directory -Force -Path $updaterDir | Out-Null
    }
    Copy-Item -Path (Join-Path $WrapperSourceDir "auto-update.ps1") -Destination $updaterDest -Force
    Write-Log "Deployed auto-updater: $updaterDest"

    # (Re)register weekly scheduled task. Sunday 3am, runs as SYSTEM with highest privileges.
    $taskName = "SunshineEnglishEdition_AutoUpdate"
    try {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
        $action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$updaterDest`""
        $trigger = New-ScheduledTaskTrigger -Weekly -DaysOfWeek Sunday -At 3am
        $principal = New-ScheduledTaskPrincipal -UserId "SYSTEM" -RunLevel Highest
        $settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
        Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description "Auto-update Foundation Sunshine - English Edition (weekly check for new wrapper releases)" | Out-Null
        Write-Log "Scheduled task '$taskName' registered (weekly Sunday 3am)."
    } catch {
        Write-Log "WARNING: failed to register auto-update task: $($_.Exception.Message). Auto-update disabled; you can re-run the wrapper manually for updates."
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
try {
    Write-Log "=== Foundation Sunshine - English Edition install start ==="
    Write-Log "InstallDir:     $InstallDir"
    Write-Log "OverlayDir:     $OverlayDir"
    Write-Log "Components:     $Components"
    Write-Log "InstallVmouse:  $InstallVmouse"

    if (-not [Environment]::Is64BitOperatingSystem) {
        Abort-Install "Foundation Sunshine requires 64-bit Windows."
    }

    $upstream = Get-UpstreamInstaller
    Invoke-UpstreamInstaller -InstallerPath $upstream
    Copy-Overlay
    Install-Vmouse
    Install-Gamepad
    Set-VersionKey
    Setup-AutoUpdate
    Restart-SunshineService

    Write-Log "=== Install complete ==="
    exit 0
} catch {
    Write-Log "FATAL: $($_.Exception.Message)"
    Write-Log $_.ScriptStackTrace
    exit 1
}
