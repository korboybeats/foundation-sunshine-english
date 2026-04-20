// ============================================================================
// wrapper.iss - Foundation Sunshine - English Edition Wrapper Installer
//
// Minimal Inno script. All install logic is in install.ps1.
// Inno just provides the GUI shell + extracts the overlay payload to {tmp}.
//
// Build:
//   iscc /DOverlayVersion=v2026.04.19-english wrapper.iss
//
// Output:
//   Output\Sunshine-EnglishEdition-Setup.exe
// ============================================================================

#ifndef OverlayVersion
  #define OverlayVersion "0.0.0-dev"
#endif

[Setup]
AppName=Foundation Sunshine - English Edition
AppVersion={#OverlayVersion}
AppPublisher=korboybeats
AppPublisherURL=https://github.com/korboybeats/foundation-sunshine-english
AppSupportURL=https://github.com/korboybeats/foundation-sunshine-english/issues
AppUpdatesURL=https://github.com/korboybeats/foundation-sunshine-english/releases
DefaultDirName={autopf}\Sunshine
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
// vmouse component removed: the ZakoVirtualMouse driver lives in a private
// AlkaidLab repo and is NOT included in any public upstream release. Ticking
// the box did nothing useful (install-vmouse.bat would fail silently because
// the .dll doesn't exist on disk).
Name: "vdd";     Description: "Virtual display driver";                       Types: full
Name: "gamepad"; Description: "Virtual gamepad driver";                       Types: full
Name: "tools";   Description: "Diagnostic tools (dxgi-info, audio-info)";     Types: full

[Files]
// install.ps1 - does all the actual work (download upstream, run silently, overlay, vmouse register)
Source: "install.ps1"; DestDir: "{tmp}"; Flags: ignoreversion deleteafterinstall
// auto-update.ps1 - shipped alongside install.ps1; install.ps1 deploys it to {app}\scripts and registers a scheduled task
Source: "auto-update.ps1"; DestDir: "{tmp}"; Flags: ignoreversion deleteafterinstall

// English overlay - staged here by build/prepare_overlay.ps1 in CI
Source: "build\overlay\sunshine.exe";          DestDir: "{tmp}\overlay";                Flags: ignoreversion deleteafterinstall
Source: "build\overlay\assets\web\*";          DestDir: "{tmp}\overlay\assets\web";     Flags: ignoreversion recursesubdirs createallsubdirs deleteafterinstall
Source: "build\overlay\scripts\vmouse\*.bat";  DestDir: "{tmp}\overlay\scripts\vmouse"; Flags: ignoreversion deleteafterinstall skipifsourcedoesntexist
Source: "build\overlay\assets\gui\sunshine-gui.exe"; DestDir: "{tmp}\overlay\assets\gui"; Flags: ignoreversion deleteafterinstall skipifsourcedoesntexist
Source: "build\overlay\OVERLAY_MANIFEST.json"; DestDir: "{tmp}\overlay";                Flags: ignoreversion deleteafterinstall

[Run]
// Run install.ps1 with all logic. Components/vmouse passed as arguments.
// Log goes to %LOCALAPPDATA%\SunshineEnglishEdition\install.log so it
// survives Inno's cleanup of {tmp}.
Filename: "powershell.exe"; \
  Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{tmp}\install.ps1"" -InstallDir ""{app}"" -OverlayDir ""{tmp}\overlay"" -Components ""{code:GetUpstreamComponents}"" {code:GetVmouseFlag} -LogPath ""{localappdata}\SunshineEnglishEdition\install.log"" -WrapperVersion ""{#OverlayVersion}"" -WrapperSourceDir ""{tmp}"""; \
  StatusMsg: "Downloading and installing Foundation Sunshine (this takes 1-2 minutes)..."; \
  Flags: runhidden waituntilterminated; \
  WorkingDir: "{tmp}"

[Code]
function GetUpstreamComponents(Param: String): String;
var
  S: String;
begin
  S := 'application,assets';
  if WizardIsComponentSelected('vdd')     then S := S + ',vdd';
  if WizardIsComponentSelected('vmouse')  then S := S + ',vmouse';
  // NOTE: 'gamepad' is intentionally NOT passed to upstream. Upstream's
  // install-gamepad.bat downloads ViGEmBus via mirror.ghproxy.com (Chinese
  // mirror, unreachable outside China) and hangs on curl's 5-min connection
  // timeout. Our own Install-Gamepad function in install.ps1 downloads
  // ViGEmBus directly from nefarius/ViGEmBus on GitHub instead.
  if WizardIsComponentSelected('tools')   then S := S + ',tools';
  Result := S;
end;

function GetVmouseFlag(Param: String): String;
begin
  if WizardIsComponentSelected('vmouse') then
    Result := '-InstallVmouse'
  else
    Result := '';
end;

function InitializeSetup(): Boolean;
begin
  Result := IsWin64();
  if not Result then
    MsgBox('Foundation Sunshine requires 64-bit Windows.', mbCriticalError, MB_OK);
end;
