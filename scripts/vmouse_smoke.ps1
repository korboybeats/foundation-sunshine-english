param(
    [string]$ProbeExe = "",
    [int]$TimeoutMs = 5000,
    [string]$MatchSubstring = "VID_1ACE&PID_0002",
    [switch]$ManualInput,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ProbeExe {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        return (Resolve-Path $ExplicitPath).Path
    }

    $candidates = @(
        (Join-Path $PSScriptRoot "..\build\tests\vmouse_probe.exe"),
        (Join-Path $PSScriptRoot "..\build-test\tests\vmouse_probe.exe"),
        (Join-Path $PSScriptRoot "..\out\build\tests\vmouse_probe.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "vmouse_probe.exe could not be found. Use -ProbeExe to specify it explicitly."
}

function Get-VMousePnpInfo {
    $device = Get-PnpDevice -InstanceId 'ROOT\HIDCLASS\*' -ErrorAction SilentlyContinue |
        Where-Object {
            ($_.FriendlyName -like '*Virtual Mouse*' -or $_.HardwareID -contains 'Root\ZakoVirtualMouse') -and
            $null -ne $_.FriendlyName -and
            $_.FriendlyName -ne ''
        } |
        Select-Object -First 1 Status, FriendlyName, Problem, InstanceId

    if ($null -eq $device) {
        return [pscustomobject]@{
            Installed  = $false
            Running    = $false
            StatusText = "Not installed"
            Device     = $null
        }
    }

    $running = $device.Status -eq "OK" -and $device.Problem -eq 0
    $statusText = if ($running) {
        "$($device.FriendlyName) - Running normally"
    }
    elseif ($device.Problem -eq 21) {
        "$($device.FriendlyName) - Restart required"
    }
    else {
        "$($device.FriendlyName) - Status=$($device.Status), Problem=$($device.Problem)"
    }

    return [pscustomobject]@{
        Installed  = $true
        Running    = $running
        StatusText = $statusText
        Device     = $device
    }
}

function Invoke-Probe {
    param([string]$ExePath, [string[]]$Arguments)

    $lines = & $ExePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $values = @{}

    foreach ($line in $lines) {
        if ($line -match '^([A-Z_]+)=(.*)$') {
            $values[$matches[1]] = $matches[2]
        }
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Lines    = $lines
        Values   = $values
    }
}

function Get-IntValue {
    param(
        [hashtable]$Table,
        [string]$Key
    )

    if ($Table.ContainsKey($Key)) {
        return [int]$Table[$Key]
    }

    return 0
}

$probePath = Resolve-ProbeExe -ExplicitPath $ProbeExe
$pnpInfo = Get-VMousePnpInfo

Write-Host "PnP status: $($pnpInfo.StatusText)"
if (-not $pnpInfo.Installed) {
    throw "Root\ZakoVirtualMouse was not detected. Please install the driver first."
}

$listResult = Invoke-Probe -ExePath $probePath -Arguments @(
    "--list-only",
    "--match-substring", $MatchSubstring
)

if (-not $Quiet) {
    $listResult.Lines | ForEach-Object { Write-Host $_ }
}

if ((Get-IntValue -Table $listResult.Values -Key "MATCHED_DEVICE_PRESENT") -ne 1) {
    throw "No matching virtual mouse was found in the Raw Input device enumeration."
}

$probeArgs = @(
    "--timeout-ms", "$TimeoutMs",
    "--match-substring", $MatchSubstring,
    "--require-device",
    "--require-events"
)

if ($ManualInput) {
    Write-Host "Within the next $TimeoutMs ms, trigger a virtual mouse move or click via Sunshine/Moonlight."
}
else {
    $probeArgs += "--send-test-sequence"
}

if ($Quiet) {
    $probeArgs += "--quiet"
}

$runResult = Invoke-Probe -ExePath $probePath -Arguments $probeArgs
if (-not $Quiet) {
    $runResult.Lines | ForEach-Object { Write-Host $_ }
}

if ($runResult.ExitCode -ne 0) {
    throw "Probe run failed with exit code $($runResult.ExitCode)."
}

$matchedEvents = Get-IntValue -Table $runResult.Values -Key "MATCHED_EVENT_COUNT"
$sendOk = Get-IntValue -Table $runResult.Values -Key "SEND_SEQUENCE_OK"

if (-not $ManualInput -and $sendOk -ne 1) {
    throw "Virtual mouse send sequence failed: could not connect to the driver or send the report."
}

if ($matchedEvents -le 0) {
    throw "No Raw Input events from the virtual mouse driver were observed."
}

Write-Host "Verification passed: driver installed, visible in Raw Input enumeration, and virtual mouse events received successfully."
