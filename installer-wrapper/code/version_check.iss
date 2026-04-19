; ============================================================================
; version_check.iss - Pre-install compatibility check
;
; Reads the wrapper's own version registry key (HKLM\SOFTWARE\SunshineEnglishEdition)
; to detect upgrades vs fresh installs. Also verifies the platform is x64.
; ============================================================================

const
  REG_KEY_PATH = 'SOFTWARE\SunshineEnglishEdition';
  REG_VALUE_VERSION = 'Version';
  REG_VALUE_INSTALLED_AT = 'InstalledAtUtc';

function GetInstalledVersion(): String;
begin
  Result := '';
  RegQueryStringValue(HKLM, REG_KEY_PATH, REG_VALUE_VERSION, Result);
end;

function IsUpgrade(): Boolean;
begin
  Result := GetInstalledVersion() <> '';
end;

procedure WriteInstalledVersion(const Version: String);
var
  NowUtc: String;
begin
  NowUtc := GetDateTimeString('yyyy-mm-dd''T''hh:nn:ss''Z''', '-', ':');
  RegWriteStringValue(HKLM, REG_KEY_PATH, REG_VALUE_VERSION, Version);
  RegWriteStringValue(HKLM, REG_KEY_PATH, REG_VALUE_INSTALLED_AT, NowUtc);
end;

function VerifyPlatformX64(): Boolean;
begin
  Result := IsWin64();
  if not Result then begin
    MsgBox(
      'Foundation Sunshine — English Edition requires 64-bit Windows.' + #13#10 +
      'This computer appears to be running 32-bit Windows; installation cannot continue.',
      mbCriticalError, MB_OK);
  end;
end;
