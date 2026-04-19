; ============================================================================
; vmouse.iss - Post-install vmouse driver registration
;
; The upstream installer drops the vmouse driver files (.dll/.inf/.cat/.cer)
; under {app}\scripts\vmouse\driver\ as part of its 'vmouse' component.
; To actually REGISTER the driver with Windows (so it shows up as a HID device),
; install-vmouse.bat must be run with admin privileges.
;
; The wrapper invokes this script if the user opted into the vmouse component.
; Failure is non-fatal — we surface a soft warning at the finish screen but
; don't roll back the install.
; ============================================================================

var
  g_VmouseInstallSucceeded: Boolean;
  g_VmouseInstallErrorMsg: String;

function InstallVmouseDriver(): Boolean;
var
  ScriptPath: String;
  ResultCode: Integer;
begin
  Result := False;
  g_VmouseInstallSucceeded := False;
  g_VmouseInstallErrorMsg := '';

  ScriptPath := ExpandConstant('{app}\scripts\vmouse\install-vmouse.bat');
  if not FileExists(ScriptPath) then begin
    g_VmouseInstallErrorMsg := 'install-vmouse.bat not found — vmouse component may not have been installed.';
    Exit;
  end;

  if Assigned(WizardForm) and Assigned(WizardForm.StatusLabel) then
    WizardForm.StatusLabel.Caption := 'Registering virtual mouse driver...';

  // Run hidden, wait for completion. Already running with admin per [Setup] PrivilegesRequired=admin.
  if not Exec(ScriptPath, '', ExpandConstant('{app}\scripts\vmouse'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
    g_VmouseInstallErrorMsg := 'Failed to launch install-vmouse.bat.';
    Exit;
  end;

  if ResultCode = 0 then begin
    g_VmouseInstallSucceeded := True;
    Result := True;
    Exit;
  end;

  g_VmouseInstallErrorMsg := Format(
    'install-vmouse.bat exited with code %d. The Sunshine install completed, ' +
    'but the virtual mouse driver may not be registered. ' +
    'You can re-run %s manually as Administrator.', [
      ResultCode, ScriptPath]);
end;
