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
Name: "vmouse";  Description: "Virtual mouse driver (anti-cheat compatible)"; Types: full
Name: "vdd";     Description: "Virtual display driver";                       Types: full
Name: "gamepad"; Description: "Virtual gamepad driver";                       Types: full
Name: "tools";   Description: "Diagnostic tools (dxgi-info, audio-info)";     Types: full

[Files]
// install.ps1 - does all the actual work (download upstream, run silently, overlay, vmouse register)
Source: "install.ps1"; DestDir: "{tmp}"; Flags: ignoreversion deleteafterinstall

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
  Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{tmp}\install.ps1"" -InstallDir ""{app}"" -OverlayDir ""{tmp}\overlay"" -Components ""{code:GetUpstreamComponents}"" {code:GetVmouseFlag} -LogPath ""{localappdata}\SunshineEnglishEdition\install.log"""; \
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
  if WizardIsComponentSelected('gamepad') then S := S + ',gamepad';
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
