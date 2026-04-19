# Wrapper Installer

A small Inno Setup project that produces `Sunshine-EnglishEdition-Setup.exe` —
the user-friendly installer for non-Chinese-speaking end users.

The wrapper composes:
1. **Upstream**: `AlkaidLab/foundation-sunshine` installer, fetched fresh from
   GitHub at install time. Provides Sunshine + drivers (VDD, vmouse, gamepad)
   + tools.
2. **Overlay**: this fork's English-translated `sunshine.exe`, web UI assets,
   and vmouse install scripts. Bundled inside the wrapper, applied after the
   upstream installer runs.

End user double-clicks the wrapper, picks components, clicks Install. Behind
the scenes: download upstream → run upstream silently → overlay English files
→ optionally register vmouse driver. Done.

## Building locally

```powershell
# Stage the English overlay (downloads latest English release)
pwsh build\prepare_overlay.ps1 -ReleaseTag v2026.04.19-english

# Compile (requires Inno Setup 6 installed)
iscc /DOverlayVersion=v2026.04.19-english wrapper.iss

# Output:
#   Output\Sunshine-EnglishEdition-Setup.exe
```

CI does this automatically on every English release publish — see
`.github/workflows/build-wrapper.yml`.

## File layout

```
installer-wrapper/
├── wrapper.iss                  Main Inno script (Setup, Files, Components, Code)
├── code/
│   ├── version_check.iss       Platform check + wrapper-version registry I/O
│   ├── download.iss            GitHub API + WinHTTP fetch with retries + cache
│   ├── upstream_runner.iss     Run AlkaidLab installer silently
│   ├── overlay_apply.iss       Verify overlay landed correctly
│   └── vmouse.iss              Run vmouse register script post-install
├── resources/                   Branding (icon, wizard images) — placeholder
├── build/
│   ├── prepare_overlay.ps1     Stage English overlay before compilation
│   └── overlay/                Generated; gitignored
└── README.md                    This file
```

## Design decisions

See `docs/INSTALLER_WRAPPER_PLAN.md` (local-only, not committed) for the full
design rationale: goals, architecture, edge cases, versioning, signing,
testing strategy, risk register.

## Maintenance

When upstream `AlkaidLab/foundation-sunshine` cuts a new release, the wrapper
auto-picks it up — no rebuild required. Only when a new **English** release is
cut does the wrapper need rebuilding (which CI handles automatically on
`release: published`).
