// ============================================================================
// upstream_runner.iss - Run the AlkaidLab installer silently
//
// Responsibilities:
//   - Build /COMPONENTS argument from user's component selection
//   - Invoke upstream installer with /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
//     /DIR="{app}" /COMPONENTS="..." /LOG="{tmp}\upstream-install.log"
//   - Capture exit code; non-zero -> set g_UpstreamRunErrorMsg
//   - Surface upstream's log path on failure
//
// Public procedures:
//   function RunUpstreamInstaller(): Boolean;
//
// Public variables:
//   g_UpstreamRunErrorMsg: String
//   g_UpstreamLogPath:     String
// ============================================================================

var
  g_UpstreamRunErrorMsg: String;
  g_UpstreamLogPath:     String;

function BuildComponentsArg(): String;
var
  Components: String;
begin
  Components := 'application,assets';

  if WizardIsComponentSelected('vdd') then
    Components := Components + ',vdd';
  if WizardIsComponentSelected('vmouse') then
    Components := Components + ',vmouse';
  if WizardIsComponentSelected('tools') then
    Components := Components + ',tools';
  if WizardIsComponentSelected('gamepad') then
    Components := Components + ',gamepad';

  Result := Components;
end;

function RunUpstreamInstaller(): Boolean;
var
  Cmd, Args, ComponentsArg, InstallDir: String;
  ResultCode: Integer;
begin
  Result := False;
  g_UpstreamRunErrorMsg := '';

  if g_UpstreamInstallerPath = '' then begin
    g_UpstreamRunErrorMsg := 'Upstream installer path not set; download must run first.';
    Exit;
  end;
  if not FileExists(g_UpstreamInstallerPath) then begin
    g_UpstreamRunErrorMsg := 'Upstream installer file missing: ' + g_UpstreamInstallerPath;
    Exit;
  end;

  InstallDir := ExpandConstant('{app}');
  ComponentsArg := BuildComponentsArg();
  g_UpstreamLogPath := ExpandConstant('{tmp}\upstream-install.log');

  Cmd := g_UpstreamInstallerPath;
  Args := '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' +
          ' /DIR="' + InstallDir + '"' +
          ' /COMPONENTS="' + ComponentsArg + '"' +
          ' /LOG="' + g_UpstreamLogPath + '"';

  if Assigned(WizardForm) and Assigned(WizardForm.StatusLabel) then
    WizardForm.StatusLabel.Caption := 'Running upstream installer (this may take a minute)...';

  if not Exec(Cmd, Args, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
    g_UpstreamRunErrorMsg := 'Failed to launch upstream installer (Exec returned False).';
    Exit;
  end;

  // Inno's exit codes: 0 success; 3010 success-needs-reboot.
  // We treat 3010 as success (the wrapper will surface needs-reboot at finish).
  if (ResultCode = 0) or (ResultCode = 3010) then begin
    Result := True;
    Exit;
  end;

  g_UpstreamRunErrorMsg := Format(
    'Upstream installer failed with exit code %d.' + #13#10 +
    'See log: %s', [
      ResultCode, g_UpstreamLogPath]);
end;
