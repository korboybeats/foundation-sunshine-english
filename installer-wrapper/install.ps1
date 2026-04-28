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
    [switch]$TrackPrereleases,
    [string]$LogPath,
    [string]$WrapperVersion = "unknown",
    [string]$WrapperSourceDir
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Initialize script-scope state so Set-StrictMode doesn't throw on first read
$script:OriginalServiceStartType = $null
$script:RestartSunshineService = $false

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
# 1. Download + extract upstream PORTABLE zip
#
# We use upstream's portable zip instead of their .exe installer because their
# installer ignores /VERYSILENT (custom Pascal in [Code] forces wizard UI).
# The portable zip ships the exact same binary set that the installer would
# extract; we just place the files ourselves into Program Files. End result
# is a normal install (service registered, drivers installed, etc.), without
# the Chinese wizard ever appearing.
# ---------------------------------------------------------------------------
function Get-UpstreamPortable {
    # Effective preference: explicit -TrackPrereleases switch wins; otherwise
    # honour the existing registry value so a silent re-install (auto-update
    # task) preserves whatever the user picked at first install.
    $usePrereleases = $TrackPrereleases.IsPresent
    if (-not $usePrereleases) {
        try {
            $existing = (Get-ItemProperty -Path "HKLM:\SOFTWARE\SunshineEnglishEdition" -Name "TrackPrereleases" -ErrorAction Stop).TrackPrereleases
            if ($existing -eq 1) { $usePrereleases = $true }
        } catch {}
    }
    Write-Log "Querying upstream release (prereleases: $usePrereleases)..."

    $headers = @{
        "User-Agent" = "SunshineEnglishEdition-Installer"
        "Accept"     = "application/vnd.github+json"
    }

    try {
        if ($usePrereleases) {
            # /releases/latest only returns stable. To include prereleases,
            # fetch the recent list and pick the most recently published
            # non-draft entry (could be stable or prerelease).
            $all = Invoke-RestMethod -Uri "https://api.github.com/repos/AlkaidLab/foundation-sunshine/releases?per_page=10" -Headers $headers -TimeoutSec 30
            $release = $all | Where-Object { -not $_.draft } | Sort-Object -Property published_at -Descending | Select-Object -First 1
            if (-not $release) { Abort-Install "No non-draft upstream releases found." }
        } else {
            $release = Invoke-RestMethod -Uri "https://api.github.com/repos/AlkaidLab/foundation-sunshine/releases/latest" -Headers $headers -TimeoutSec 30
        }
    } catch {
        Abort-Install "Failed to query upstream release: $($_.Exception.Message)"
    }

    Write-Log "Upstream tag: $($release.tag_name) (prerelease: $($release.prerelease))"

    $asset = $release.assets | Where-Object { $_.name -like "*Portable*.zip" } | Select-Object -First 1
    if (-not $asset) {
        Abort-Install "No *Portable*.zip asset in upstream release $($release.tag_name)"
    }

    Write-Log "Found asset: $($asset.name) ($([math]::Round($asset.size / 1MB, 1)) MB)"

    $cacheDir = Join-Path $env:LOCALAPPDATA "SunshineEnglishEdition\cache"
    if (-not (Test-Path $cacheDir)) {
        New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
    }
    $cachePath = Join-Path $cacheDir $asset.name

    if (-not (Test-Path $cachePath)) {
        Write-Log "Downloading from $($asset.browser_download_url) ..."
        $maxAttempts = 3
        $downloaded = $false
        for ($i = 1; $i -le $maxAttempts; $i++) {
            try {
                Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $cachePath -Headers $headers -TimeoutSec 600
                Write-Log "Download succeeded ($([math]::Round((Get-Item $cachePath).Length / 1MB, 1)) MB)"
                $downloaded = $true
                break
            } catch {
                Write-Log "Attempt $i/$maxAttempts failed: $($_.Exception.Message)"
                if ($i -lt $maxAttempts) {
                    $backoff = 5 * $i * $i
                    Write-Log "Retrying in $backoff seconds..."
                    Start-Sleep -Seconds $backoff
                }
            }
        }
        if (-not $downloaded) {
            Abort-Install "Failed to download upstream portable zip after $maxAttempts attempts."
        }
    } else {
        Write-Log "Using cached portable zip: $cachePath"
    }

    # Extract to a fresh staging directory under TEMP
    $stagingDir = Join-Path $env:TEMP "SunshineEnglishEdition-portable-staging"
    if (Test-Path $stagingDir) { Remove-Item -Recurse -Force $stagingDir }
    New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

    Write-Log "Extracting portable zip to $stagingDir ..."
    try {
        Expand-Archive -Path $cachePath -DestinationPath $stagingDir -Force
    } catch {
        Abort-Install "Failed to extract portable zip: $($_.Exception.Message)"
    }

    # The zip has a single 'Sunshine/' top-level folder containing all the files.
    $sunshineRoot = Get-ChildItem -Path $stagingDir -Directory | Select-Object -First 1
    if (-not $sunshineRoot) {
        Abort-Install "Extracted zip has unexpected layout (no top-level dir)."
    }
    Write-Log "Extracted to $($sunshineRoot.FullName)"

    return @{
        ExtractedDir = $sunshineRoot.FullName
        ReleaseTag   = $release.tag_name
        PublishedAt  = $release.published_at
        IsPrerelease = [bool]$release.prerelease
        HtmlUrl      = $release.html_url
    }
}

# ---------------------------------------------------------------------------
# 2. Install from extracted portable zip
#
# Copies all files from the extracted Sunshine/ directory into $InstallDir,
# preserving user config (we never touch $InstallDir\config\). On first
# install, also runs upstream's setup scripts (install-service.bat,
# install-vdd.bat) to register the SunshineService and install drivers.
# ---------------------------------------------------------------------------
function Install-FromPortable {
    param(
        [Parameter(Mandatory)] [string]$ExtractedDir,
        [Parameter(Mandatory)] [bool]$IsFirstInstall
    )

    Stop-SunshineProcesses

    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    }

    # Copy everything from the extracted portable dir to InstallDir.
    # We explicitly do NOT touch $InstallDir\config\ which holds user data
    # (sunshine.conf, apps.json, covers/, credentials, etc.). The portable
    # zip doesn't include a config/ subdirectory, so this is safe by default.
    Write-Log "Copying upstream portable files $ExtractedDir -> $InstallDir ..."
    $count = 0
    Get-ChildItem -Path $ExtractedDir -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($ExtractedDir.Length).TrimStart('\', '/')
        # Defensive: skip any path that lands inside config/ (shouldn't happen
        # with current portable zip, but in case upstream changes layout)
        if ($relative -match '(?i)^config[/\\]') { return }
        $dest = Join-Path $InstallDir $relative
        $destDir = Split-Path -Parent $dest
        if (-not (Test-Path $destDir)) {
            New-Item -ItemType Directory -Force -Path $destDir | Out-Null
        }
        $copied = $false
        for ($attempt = 1; $attempt -le 3 -and -not $copied; $attempt++) {
            try {
                Copy-Item -Path $_.FullName -Destination $dest -Force -ErrorAction Stop
                $copied = $true
            } catch {
                Write-Log "  Copy attempt $attempt failed for $relative; killing Sunshine and retrying..."
                Stop-SunshineProcesses
            }
        }
        if (-not $copied) {
            Abort-Install "Failed to copy $relative from portable zip after 3 attempts."
        }
        $count++
    }
    Write-Log "Copied $count files from upstream portable."

    # Sanity check
    $marker = Join-Path $InstallDir "sunshine.exe"
    if (-not (Test-Path $marker)) {
        Abort-Install "Upstream copy verification failed: $marker missing."
    }
    Write-Log "Upstream sunshine.exe in place."

    if ($IsFirstInstall) {
        Write-Log "First install detected; running upstream setup scripts..."

        # Install SunshineService
        $svcBat = Join-Path $InstallDir "scripts\install-service.bat"
        if (Test-Path $svcBat) {
            Write-Log "  Running install-service.bat ..."
            $proc = Start-Process -FilePath $svcBat -WorkingDirectory (Split-Path $svcBat) -Wait -PassThru -NoNewWindow
            Write-Log "  install-service.bat exit code: $($proc.ExitCode)"
        } else {
            Write-Log "  WARN: install-service.bat not found in portable zip."
        }

        # Install VDD driver if vdd component selected
        if ($Components -match '\bvdd\b') {
            $vddBat = Join-Path $InstallDir "scripts\install-vdd.bat"
            if (Test-Path $vddBat) {
                Write-Log "  Running install-vdd.bat ..."
                $proc = Start-Process -FilePath $vddBat -WorkingDirectory (Split-Path $vddBat) -Wait -PassThru -NoNewWindow
                Write-Log "  install-vdd.bat exit code: $($proc.ExitCode)"
            } else {
                Write-Log "  WARN: install-vdd.bat not found in portable zip."
            }
        }

        # Add firewall rules
        $fwBat = Join-Path $InstallDir "scripts\add-firewall-rule.bat"
        if (Test-Path $fwBat) {
            Write-Log "  Running add-firewall-rule.bat ..."
            $proc = Start-Process -FilePath $fwBat -WorkingDirectory (Split-Path $fwBat) -Wait -PassThru -NoNewWindow
            Write-Log "  add-firewall-rule.bat exit code: $($proc.ExitCode)"
        }

        # Update PATH
        $pathBat = Join-Path $InstallDir "scripts\update-path.bat"
        if (Test-Path $pathBat) {
            Write-Log "  Running update-path.bat add ..."
            $proc = Start-Process -FilePath $pathBat -ArgumentList "add" -WorkingDirectory (Split-Path $pathBat) -Wait -PassThru -NoNewWindow
            Write-Log "  update-path.bat exit code: $($proc.ExitCode)"
        }
    } else {
        Write-Log "Existing install detected; skipping first-install setup scripts."
    }
}

# ---------------------------------------------------------------------------
# 2b. Force English locale in sunshine.conf
# ---------------------------------------------------------------------------
function Set-EnglishLocale {
    # Set both `locale` (web UI / log messages) and `tray_locale` (system tray
    # menu) to English in the user's sunshine.conf, creating the file/section
    # entries as needed. Preserves all other settings the user has configured.
    $configPath = Join-Path $InstallDir "config\sunshine.conf"
    if (-not (Test-Path $configPath)) {
        Write-Log "sunshine.conf not present at $configPath; will be created with English defaults on first launch."
        # Create a minimal config that pre-sets English
        $configDir = Split-Path -Parent $configPath
        if (-not (Test-Path $configDir)) {
            New-Item -ItemType Directory -Force -Path $configDir | Out-Null
        }
        Set-Content -Path $configPath -Value "locale = en_US`r`ntray_locale = en`r`n" -Encoding UTF8
        Write-Log "Created $configPath with English locale defaults."
        return
    }

    $content = Get-Content -Path $configPath -Raw
    $changed = $false

    if ($content -match '(?m)^locale\s*=') {
        if ($content -notmatch '(?m)^locale\s*=\s*en') {
            $content = [regex]::Replace($content, '(?m)^locale\s*=.*$', 'locale = en_US')
            $changed = $true
        }
    } else {
        $content += "`r`nlocale = en_US`r`n"
        $changed = $true
    }

    if ($content -match '(?m)^tray_locale\s*=') {
        if ($content -notmatch '(?m)^tray_locale\s*=\s*en') {
            $content = [regex]::Replace($content, '(?m)^tray_locale\s*=.*$', 'tray_locale = en')
            $changed = $true
        }
    } else {
        $content += "tray_locale = en`r`n"
        $changed = $true
    }

    if ($changed) {
        Set-Content -Path $configPath -Value $content -Encoding UTF8 -NoNewline
        Write-Log "Forced English locale in $configPath (locale=en_US, tray_locale=en)."
    } else {
        Write-Log "sunshine.conf already English (locale=en_*, tray_locale=en)."
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
    # Restore service startup type then start it. Always attempt to start
    # unconditionally — the conditional $script:RestartSunshineService flag
    # was unreliable (second Stop-SunshineProcesses call could leave the flag
    # in an unexpected state, causing the service to remain stopped after install).
    $svc = Get-Service -Name "SunshineService" -ErrorAction SilentlyContinue
    if (-not $svc) { return }
    $restoreType = $script:OriginalServiceStartType
    if (-not $restoreType -or "$restoreType" -eq 'Disabled') { $restoreType = 'Automatic' }
    try {
        Set-Service -Name "SunshineService" -StartupType $restoreType -ErrorAction Stop
        Write-Log "Restored SunshineService startup type to $restoreType."
    } catch {
        Write-Log "WARN: failed to restore SunshineService startup type: $($_.Exception.Message)"
    }
    try {
        Start-Service -Name "SunshineService" -ErrorAction Stop
        Write-Log "SunshineService started."
    } catch {
        Write-Log "WARN: failed to start SunshineService: $($_.Exception.Message). Start manually if needed."
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
    param([hashtable]$Portable)

    $key = "HKLM:\SOFTWARE\SunshineEnglishEdition"
    if (-not (Test-Path $key)) {
        New-Item -Path $key -Force | Out-Null
    }
    Set-ItemProperty -Path $key -Name "Version" -Value $WrapperVersion
    Set-ItemProperty -Path $key -Name "InstalledAtUtc" -Value (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")

    # Track-prereleases preference: explicit switch wins; else preserve the
    # existing value (so silent re-installs from auto-update don't reset it).
    if ($TrackPrereleases.IsPresent) {
        Set-ItemProperty -Path $key -Name "TrackPrereleases" -Value 1 -Type DWord
    } else {
        # Only initialise the value if it doesn't exist; never overwrite a
        # previously-set "1" via a silent reinstall.
        try { (Get-ItemProperty -Path $key -Name "TrackPrereleases" -ErrorAction Stop) | Out-Null } catch {
            Set-ItemProperty -Path $key -Name "TrackPrereleases" -Value 0 -Type DWord
        }
    }

    if ($Portable) {
        Set-ItemProperty -Path $key -Name "UpstreamTag" -Value $Portable.ReleaseTag
        Set-ItemProperty -Path $key -Name "UpstreamIsPrerelease" -Value ([int][bool]$Portable.IsPrerelease) -Type DWord
    }

    Write-Log "Wrote registry: Version=$WrapperVersion; UpstreamTag=$($Portable.ReleaseTag); TrackPrereleases=$($TrackPrereleases.IsPresent)"
}

# ---------------------------------------------------------------------------
# Write upstream version metadata to the Web UI assets dir so the Web UI
# can display the actual upstream Sunshine version (instead of the raw
# 0.0.0.<commit> FileVersion that sunshine.exe reports). Sunshine's
# embedded HTTP server serves assets/web/ at the root, so the file is
# fetchable from the browser at /upstream_version.json.
# ---------------------------------------------------------------------------
function Write-UpstreamVersionFile {
    param([Parameter(Mandatory)] [hashtable]$Portable)

    $webDir = Join-Path $InstallDir "assets\web"
    if (-not (Test-Path $webDir)) {
        Write-Log "WARN: $webDir not found; skipping upstream_version.json write."
        return
    }
    $payload = [ordered]@{
        upstream_tag        = $Portable.ReleaseTag
        upstream_published  = $Portable.PublishedAt
        upstream_prerelease = [bool]$Portable.IsPrerelease
        upstream_url        = $Portable.HtmlUrl
        wrapper_version     = $WrapperVersion
        installed_at_utc    = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    }
    $dest = Join-Path $webDir "upstream_version.json"
    $payload | ConvertTo-Json -Depth 4 | Set-Content -Path $dest -Encoding UTF8
    Write-Log "Wrote $dest"
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

    # Detect first install vs update by checking if SunshineService exists.
    # First install needs the setup scripts (service, VDD, firewall, PATH);
    # updates only need to overwrite files.
    $existingService = Get-Service -Name "SunshineService" -ErrorAction SilentlyContinue
    $isFirstInstall = -not $existingService
    Write-Log "Install mode: $(if ($isFirstInstall) { 'FIRST INSTALL' } else { 'UPDATE' })"

    # Always download + extract latest upstream portable zip (auto-current
    # with upstream releases). Then copy contents to InstallDir.
    $portable = Get-UpstreamPortable
    Install-FromPortable -ExtractedDir $portable.ExtractedDir -IsFirstInstall $isFirstInstall

    # Apply English overlay (assets/web/, sunshine-gui.exe, vmouse scripts)
    # on top of the upstream files we just copied.
    Copy-Overlay

    # Force locale=en_US, tray_locale=en in sunshine.conf
    Set-EnglishLocale

    Install-Vmouse
    Install-Gamepad
    Set-VersionKey -Portable $portable
    Write-UpstreamVersionFile -Portable $portable
    Setup-AutoUpdate
    Restart-SunshineService

    Write-Log "=== Install complete ==="
    exit 0
} catch {
    Write-Log "FATAL: $($_.Exception.Message)"
    Write-Log $_.ScriptStackTrace
    exit 1
}
