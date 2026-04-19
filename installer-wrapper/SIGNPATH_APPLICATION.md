# SignPath OSS Code Signing Application Draft

This is a draft of the application to submit to https://signpath.org/oss for free
EV code-signing of the wrapper installer. Review and submit yourself —
SignPath needs to receive it from the project owner, not me.

---

## Apply at

https://signpath.org/sign-up-as-an-open-source-project

## Form fields

### Project name
Foundation Sunshine — English Edition

### Project URL
https://github.com/korboybeats/foundation-sunshine-english

### License
GPLv3 (inherited from upstream LizardByte/Sunshine)

### What is this project?

Foundation Sunshine — English Edition is a downstream localization of the
[AlkaidLab/foundation-sunshine](https://github.com/AlkaidLab/foundation-sunshine)
fork of [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine), a
self-hosted game-stream host for Moonlight. The upstream fork (in Chinese)
adds HDR support, virtual display integration, and other Windows-specific
streaming features. This project translates the user-facing strings, system
tray, log messages, web UI, build scripts, and documentation to English so
non-Chinese-speaking gamers can use the fork.

The artifact we want signed is `Sunshine-EnglishEdition-Setup.exe`, an Inno
Setup wrapper installer that:

1. Downloads the upstream AlkaidLab installer fresh from GitHub at install time
2. Runs it silently
3. Overlays our English-translated `sunshine.exe` and web assets on top
4. Optionally registers the virtual mouse driver

End users get a one-click install of a fully English Foundation Sunshine.
Source code, build pipeline, and release process are all open and
reproducible.

### Why do you need code signing?

Without a signed installer, Windows SmartScreen displays an
"Unrecognized publisher" warning that scares away non-technical end users.
Many of our target users (gamers, streamers) are not comfortable clicking
through "More info → Run anyway" — they assume the installer is malware
and abandon the install.

This project is built and published entirely on GitHub Actions. No
maintainer machine ever has the signing key.

### Build pipeline / signing integration plan

Builds run on `windows-latest` GitHub Actions runners via
[`build-wrapper.yml`](https://github.com/korboybeats/foundation-sunshine-english/blob/english-translation/.github/workflows/build-wrapper.yml).

The pipeline:
1. Stages the English overlay from the latest English release (via gh CLI)
2. Compiles `wrapper.iss` with Inno Setup 6.4.3 (installed via Chocolatey)
3. Produces `Sunshine-EnglishEdition-Setup.exe` (~30 MB)
4. (Future) Signs via SignPath using the official `signpath/github-action-submit-signing-request@v1` action
5. Uploads signed artifact to the GitHub release

We will follow the standard SignPath GitHub Actions integration pattern.
Origin-binding will be set to this repository + the `build-wrapper.yml`
workflow on the `english-translation` branch.

### How will you ensure responsible signing?

- **Only the wrapper installer is signed** — no arbitrary binaries get
  signed. The `[Files]` section of `wrapper.iss` is reviewable in the repo;
  it bundles only files extracted from the corresponding English release
  (`sunshine.exe`, `assets/web/`, vmouse `.bat` scripts).
- **The English release is itself a CI build** from
  [`main.yml`](https://github.com/korboybeats/foundation-sunshine-english/blob/english-translation/.github/workflows/main.yml)
  — no maintainer-uploaded blobs.
- **Origin-binding** will restrict signing to this repository, this
  workflow, and the `english-translation` branch.
- **Approval mode** — set to require manual approval for first ~10 builds,
  then transition to automatic if no concerns surface.

### Project owner

- **Name**: korboybeats
- **GitHub**: https://github.com/korboybeats
- **Email**: agentcode1188@gmail.com
- **Time zone**: (fill in)

### Anything else?

The vmouse virtual mouse driver in this project is **not** signed by us;
it comes from the upstream private repo `AlkaidLab/ZakoVirtualMouse` and
is signed by them. We only sign the outer wrapper installer. The user's
own `nefconw.exe` (also from upstream) registers the driver during install
using upstream's existing signature.

---

## After acceptance

Add a follow-up commit to the wrapper workflow:

```yaml
- name: Submit to SignPath for signing
  uses: signpath/github-action-submit-signing-request@v1
  with:
    api-token: ${{ secrets.SIGNPATH_API_TOKEN }}
    organization-id: '<your-org-id>'
    project-slug: 'foundation-sunshine-english'
    signing-policy-slug: 'release-signing'
    artifact-configuration-slug: 'wrapper-installer'
    github-artifact-id: ${{ steps.upload.outputs.artifact-id }}
    wait-for-completion: true
    output-artifact-directory: 'signed/'
```

Plus add `SIGNPATH_API_TOKEN` to repo secrets.
