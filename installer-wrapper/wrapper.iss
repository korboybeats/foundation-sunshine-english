; ============================================================================
; wrapper.iss - Foundation Sunshine — English Edition Wrapper Installer
;
; Composes upstream AlkaidLab/foundation-sunshine + this fork's English overlay
; into a single user-friendly installer.
;
; Build:
;   iscc /DOverlayVersion=v2026.04.19-english wrapper.iss
;
; Output:
;   Output\Sunshine-EnglishEdition-Setup.exe
; ============================================================================

#ifndef OverlayVersion
  #define OverlayVersion "0.0.0-dev"
#endif

[Setup]
AppName=Foundation Sunshine — English Edition
AppVersion={#OverlayVersion}
AppPublisher=korboybeats
AppPublisherURL=https://github.com/korboybeats/foundation-sunshine-english
AppSupportURL=https://github.com/korboybeats/foundation-sunshine-english/issues
AppUpdatesURL=https://github.com/korboybeats/foundation-sunshine-english/releases
DefaultDirName={autopf}\Sunshine
DefaultGroupName=Foundation Sunshine
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename=Sunshine-EnglishEdition-Setup
OutputDir=Output
Compression=lzma2/ultra64
SolidCompression=yes
SetupLogging=yes
WizardStyle=modern
LicenseFile=..\LICENSE
UninstallDisplayName=Foundation Sunshine (English Edition)
UninstallDisplayIcon={app}\sunshine.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "english";  Description: "English UI (sunshine.exe + web assets)";        Types: full custom; Flags: fixed
Name: "upstream"; Description: "AlkaidLab base build (downloaded at install)";   Types: full custom; Flags: fixed
Name: "vmouse";   Description: "Virtual mouse driver (anti-cheat compatible)";   Types: full
Name: "vdd";      Description: "Virtual display driver";                         Types: full
Name: "gamepad";  Description: "Virtual gamepad driver";                         Types: full
Name: "tools";    Description: "Diagnostic tools (dxgi-info, audio-info)";       Types: full

[Files]
; Embedded English overlay - extracted from latest English release at build time
; by build/prepare_overlay.ps1. These files are copied AFTER the upstream installer
; runs, overwriting the upstream's Chinese-string equivalents.
Source: "build\overlay\sunshine.exe";          DestDir: "{app}";                  Components: english; Flags: ignoreversion
Source: "build\overlay\assets\web\*";          DestDir: "{app}\assets\web";       Components: english; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\overlay\scripts\vmouse\*.bat";  DestDir: "{app}\scripts\vmouse";   Components: english; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\overlay\OVERLAY_MANIFEST.json"; DestDir: "{app}";                  Components: english; Flags: ignoreversion

[Icons]
Name: "{group}\Foundation Sunshine"; Filename: "{app}\sunshine.exe"
Name: "{group}\Uninstall Foundation Sunshine (English)"; Filename: "{uninstallexe}"

[Code]
#include "code\version_check.iss"
#include "code\download.iss"
#include "code\upstream_runner.iss"
#include "code\overlay_apply.iss"
#include "code\vmouse.iss"

var
  g_PostInstallSummary: TStringList;

function InitializeSetup(): Boolean;
begin
  Result := VerifyPlatformX64();
end;

procedure InitializeWizard();
begin
  g_PostInstallSummary := TStringList.Create();
end;

procedure DeinitializeSetup();
begin
  if Assigned(g_PostInstallSummary) then
    g_PostInstallSummary.Free();
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then begin
    // ssInstall fires AFTER the user clicks Install but BEFORE Inno copies the
    // [Files] section. We do the upstream download + run here so the upstream
    // installer drops files into {app}, then Inno's [Files] copy overlays our
    // English files on top.

    if WizardIsComponentSelected('upstream') then begin
      if not DownloadUpstreamInstaller() then begin
        MsgBox(
          'Failed to download the upstream Foundation Sunshine installer:' + #13#10#13#10 +
          g_DownloadErrorMsg + #13#10#13#10 +
          'Please check your internet connection and try again. ' +
          'You can also download the upstream installer manually from ' +
          'https://github.com/AlkaidLab/foundation-sunshine/releases/latest ' +
          'and install it before running this wrapper.',
          mbCriticalError, MB_OK);
        Abort();
      end;
      g_PostInstallSummary.Add('+ Upstream installer: ' + g_UpstreamReleaseTag);

      if not RunUpstreamInstaller() then begin
        MsgBox(
          'The upstream installer reported a failure:' + #13#10#13#10 +
          g_UpstreamRunErrorMsg,
          mbCriticalError, MB_OK);
        Abort();
      end;
      g_PostInstallSummary.Add('+ Upstream install completed.');
    end;
  end
  else if CurStep = ssPostInstall then begin
    // ssPostInstall fires AFTER Inno copies the overlay files. Verify the overlay
    // landed where we expected it, then run vmouse register if requested.

    if WizardIsComponentSelected('english') then begin
      if VerifyOverlayApplied() then begin
        g_PostInstallSummary.Add('+ English overlay applied.');
      end else begin
        MsgBox(
          'Warning: English overlay verification failed.' + #13#10#13#10 +
          g_OverlayApplyErrorMsg,
          mbInformation, MB_OK);
        g_PostInstallSummary.Add('! English overlay verification failed.');
      end;
    end;

    if WizardIsComponentSelected('vmouse') then begin
      if InstallVmouseDriver() then begin
        g_PostInstallSummary.Add('+ Virtual mouse driver registered.');
      end else begin
        MsgBox(
          'Note: Virtual mouse driver setup did not complete:' + #13#10#13#10 +
          g_VmouseInstallErrorMsg + #13#10#13#10 +
          'Foundation Sunshine itself is installed and working. The virtual mouse ' +
          'feature (used for some anti-cheat-protected games) will be unavailable. ' +
          'You can re-run install-vmouse.bat manually later.',
          mbInformation, MB_OK);
        g_PostInstallSummary.Add('! vmouse register failed (non-fatal).');
      end;
    end;

    WriteInstalledVersion('{#OverlayVersion}');
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  // On the Finished page, replace the default message with our install summary.
  if (CurPageID = wpFinished) and Assigned(g_PostInstallSummary) and (g_PostInstallSummary.Count > 0) then begin
    WizardForm.FinishedLabel.Caption :=
      'Foundation Sunshine — English Edition has been installed.' + #13#10#13#10 +
      'Install summary:' + #13#10 +
      g_PostInstallSummary.Text + #13#10 +
      'You can launch Sunshine from the Start menu or your installation directory.';
  end;
end;
