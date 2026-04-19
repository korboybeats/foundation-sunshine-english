<#
.SYNOPSIS
    Prepares the English overlay file set for embedding into the wrapper installer.

.DESCRIPTION
    Downloads the English release portable zip from korboybeats/foundation-sunshine-english,
    extracts the files that the wrapper installer overlays on top of the upstream
    AlkaidLab/foundation-sunshine install (sunshine.exe, assets/web/, vmouse .bat scripts),
    and writes a manifest with the source release tag for traceability.

    Run from CI before invoking iscc on wrapper.iss. The output goes into
    installer-wrapper/build/overlay/ which wrapper.iss references in its [Files] section.

.PARAMETER ReleaseTag
    English release tag to use (e.g. "v2026.04.19-english"). Defaults to latest release
    if the ENGLISH_RELEASE_TAG env var is unset.

.PARAMETER Repo
    GitHub repo to pull the release from. Defaults to korboybeats/foundation-sunshine-english.

.EXAMPLE
    pwsh installer-wrapper/build/prepare_overlay.ps1
    # Uses ENGLISH_RELEASE_TAG env var, or latest if unset.

.EXAMPLE
    pwsh installer-wrapper/build/prepare_overlay.ps1 -ReleaseTag v2026.04.19-english
#>

[CmdletBinding()]
param(
    [string]$ReleaseTag = $env:ENGLISH_RELEASE_TAG,
    [string]$Repo = "korboybeats/foundation-sunshine-english"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$WrapperRoot = Split-Path -Parent $ScriptRoot
$OverlayDir = Join-Path $WrapperRoot "build/overlay"
$WorkDir = Join-Path $WrapperRoot "build/_work"

function Write-Step([string]$Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Resolve-LatestReleaseTag {
    Write-Host "Querying $Repo for latest release..."
    $release = gh release view --repo $Repo --json tagName,name | ConvertFrom-Json
    if (-not $release.tagName) {
        throw "Could not resolve latest release tag for $Repo"
    }
    return $release.tagName
}

function Get-PortableZipUrl([string]$Tag) {
    Write-Host "Listing assets for $Tag..."
    $assets = gh release view $Tag --repo $Repo --json assets | ConvertFrom-Json
    $portable = $assets.assets | Where-Object { $_.name -like "*Portable*.zip" } | Select-Object -First 1
    if (-not $portable) {
        throw "No *Portable*.zip asset found in release $Tag"
    }
    Write-Host "Selected asset: $($portable.name) ($([math]::Round($portable.size / 1MB, 1)) MB)"
    return @{
        Name = $portable.name
        Url  = $portable.url
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
Write-Step "Resolving English release tag"
if ([string]::IsNullOrWhiteSpace($ReleaseTag) -or $ReleaseTag -eq "latest") {
    $ReleaseTag = Resolve-LatestReleaseTag
}
Write-Host "Using release tag: $ReleaseTag"

Write-Step "Locating portable zip asset"
$asset = Get-PortableZipUrl -Tag $ReleaseTag

Write-Step "Preparing work directories"
if (Test-Path $OverlayDir) { Remove-Item -Recurse -Force $OverlayDir }
if (Test-Path $WorkDir) { Remove-Item -Recurse -Force $WorkDir }
New-Item -ItemType Directory -Force -Path $OverlayDir | Out-Null
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

Write-Step "Downloading $($asset.Name)"
$zipPath = Join-Path $WorkDir $asset.Name
gh release download $ReleaseTag --repo $Repo --pattern $asset.Name --dir $WorkDir --clobber
if (-not (Test-Path $zipPath)) {
    throw "Download failed; expected file at $zipPath"
}
$zipSize = (Get-Item $zipPath).Length
Write-Host "Downloaded $([math]::Round($zipSize / 1MB, 1)) MB"

Write-Step "Extracting portable zip"
$extractDir = Join-Path $WorkDir "extracted"
Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force

# Portable zip contains a Sunshine/ root folder; locate it
$sunshineRoot = Get-ChildItem -Path $extractDir -Directory | Select-Object -First 1
if (-not $sunshineRoot) {
    throw "Extracted zip has no top-level directory; layout unexpected"
}
Write-Host "Source root: $($sunshineRoot.FullName)"

Write-Step "Staging overlay files"

# Files we overlay on top of the upstream install. These are exactly the files
# whose content differs between English fork and upstream — anything not in this
# list (drivers, tools, sunshine-gui, etc.) comes from the upstream installer.

$overlayItems = @(
    @{ Source = "sunshine.exe"; Destination = "sunshine.exe"; Type = "File" }
    @{ Source = "assets\web";   Destination = "assets\web";   Type = "Directory" }
    @{ Source = "scripts\vmouse"; Destination = "scripts\vmouse"; Type = "Directory"; Filter = "*.bat" }
)

foreach ($item in $overlayItems) {
    $src = Join-Path $sunshineRoot.FullName $item.Source
    $dst = Join-Path $OverlayDir $item.Destination

    if (-not (Test-Path $src)) {
        Write-Warning "Overlay source missing: $src — skipping"
        continue
    }

    $dstParent = Split-Path -Parent $dst
    if (-not (Test-Path $dstParent)) {
        New-Item -ItemType Directory -Force -Path $dstParent | Out-Null
    }

    if ($item.Type -eq "File") {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "  + $($item.Destination)"
    } else {
        # Directory copy with optional filter
        if ($item.ContainsKey("Filter")) {
            New-Item -ItemType Directory -Force -Path $dst | Out-Null
            Get-ChildItem -Path $src -Filter $item.Filter -File | ForEach-Object {
                Copy-Item -Path $_.FullName -Destination $dst -Force
                Write-Host "  + $($item.Destination)\$($_.Name)"
            }
        } else {
            Copy-Item -Path $src -Destination $dst -Recurse -Force
            $count = (Get-ChildItem -Path $dst -Recurse -File | Measure-Object).Count
            Write-Host "  + $($item.Destination)\ ($count files)"
        }
    }
}

Write-Step "Writing manifest"
$manifest = [ordered]@{
    English_Release_Tag = $ReleaseTag
    Source_Asset        = $asset.Name
    Generated_At_UTC    = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ").ToString()
    Repo                = $Repo
    Files_Count         = (Get-ChildItem -Path $OverlayDir -Recurse -File | Measure-Object).Count
}
$manifestPath = Join-Path $OverlayDir "OVERLAY_MANIFEST.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding UTF8
Write-Host "Manifest: $manifestPath"
Get-Content $manifestPath

Write-Step "Cleanup"
Remove-Item -Recurse -Force $WorkDir
Write-Host "Removed work directory."

Write-Step "Done"
$totalSize = (Get-ChildItem -Path $OverlayDir -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host "Overlay staged at: $OverlayDir"
Write-Host "Total size: $([math]::Round($totalSize / 1MB, 1)) MB"
