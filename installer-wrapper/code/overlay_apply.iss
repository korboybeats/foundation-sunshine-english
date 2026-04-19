; ============================================================================
; overlay_apply.iss - Verify English overlay landed correctly
;
; The actual overlay file copying is handled by Inno's [Files] section using
; standard Source/DestDir directives with the ignoreversion flag — those run
; automatically as part of normal Inno installation.
;
; This module provides a post-overlay sanity check that the marker files
; (sunshine.exe, assets/web/index.html) actually exist in {app} where we
; expect them. If they don't, something went wrong with the upstream install
; (probably a different default install dir) and the wrapper failed silently.
; ============================================================================

var
  g_OverlayApplyErrorMsg: String;

function VerifyOverlayApplied(): Boolean;
var
  AppDir, MarkerExe, MarkerWeb: String;
  MissingItems: String;
begin
  AppDir := ExpandConstant('{app}');
  MarkerExe := AppDir + '\sunshine.exe';
  MarkerWeb := AppDir + '\assets\web\config.html';

  MissingItems := '';
  if not FileExists(MarkerExe) then
    MissingItems := MissingItems + #13#10 + '  - sunshine.exe';
  if not FileExists(MarkerWeb) then
    MissingItems := MissingItems + #13#10 + '  - assets\web\config.html';

  if MissingItems <> '' then begin
    g_OverlayApplyErrorMsg := Format(
      'Overlay verification failed: expected files missing in %s:%s' + #13#10 +
      'This usually means the upstream installer used a different install directory ' +
      'than expected. Try uninstalling Sunshine first, then re-run this installer.', [
        AppDir, MissingItems]);
    Result := False;
    Exit;
  end;

  Result := True;
end;
