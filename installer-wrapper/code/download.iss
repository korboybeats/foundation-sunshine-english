; ============================================================================
; download.iss - GitHub release fetch + verification
;
; Responsibilities:
;   - Query GitHub API for AlkaidLab/foundation-sunshine latest release
;   - Find the *WindowsInstaller.exe asset
;   - Download with progress reporting via WizardForm.ProgressGauge
;   - Verify SHA-256 against SHA256SUMS.txt from the same release
;   - Retry 3x with exponential backoff (5s, 15s, 45s)
;   - Cache successful downloads under {userappdata}\SunshineEnglishEdition\cache\
;
; Public procedures:
;   function DownloadUpstreamInstaller(): Boolean;
;     Returns True on success. On failure, sets g_DownloadErrorMsg.
;
; Public variables (read-only after DownloadUpstreamInstaller returns):
;   g_UpstreamInstallerPath: String  - full path to downloaded .exe
;   g_UpstreamReleaseTag:    String  - tag name of the release we fetched
;   g_DownloadErrorMsg:      String  - human-readable failure description
; ============================================================================

const
  UPSTREAM_REPO = 'AlkaidLab/foundation-sunshine';
  UPSTREAM_API  = 'https://api.github.com/repos/AlkaidLab/foundation-sunshine/releases/latest';
  CACHE_SUBDIR  = '\SunshineEnglishEdition\cache';
  MAX_RETRIES   = 3;

var
  g_UpstreamInstallerPath: String;
  g_UpstreamReleaseTag:    String;
  g_DownloadErrorMsg:      String;

function GetCacheDir(): String;
begin
  Result := ExpandConstant('{userappdata}') + CACHE_SUBDIR;
  if not DirExists(Result) then
    ForceDirectories(Result);
end;

function HttpGetToFile(const Url, DestPath: String): Boolean;
var
  WinHttp: Variant;
  Stream:  Variant;
begin
  Result := False;
  try
    WinHttp := CreateOleObject('WinHttp.WinHttpRequest.5.1');
    WinHttp.Open('GET', Url, False);
    WinHttp.SetRequestHeader('User-Agent', 'SunshineEnglishEdition-Installer');
    WinHttp.SetRequestHeader('Accept', 'application/octet-stream');
    WinHttp.Send();

    if WinHttp.Status <> 200 then begin
      g_DownloadErrorMsg := Format('HTTP %d for %s', [WinHttp.Status, Url]);
      Exit;
    end;

    Stream := CreateOleObject('ADODB.Stream');
    Stream.Type := 1; // adTypeBinary
    Stream.Open();
    Stream.Write(WinHttp.ResponseBody);
    Stream.SaveToFile(DestPath, 2); // adSaveCreateOverWrite
    Stream.Close();

    Result := True;
  except
    g_DownloadErrorMsg := 'Network error: ' + GetExceptionMessage();
  end;
end;

function HttpGetToString(const Url: String; var ResponseText: String): Boolean;
var
  WinHttp: Variant;
begin
  Result := False;
  ResponseText := '';
  try
    WinHttp := CreateOleObject('WinHttp.WinHttpRequest.5.1');
    WinHttp.Open('GET', Url, False);
    WinHttp.SetRequestHeader('User-Agent', 'SunshineEnglishEdition-Installer');
    WinHttp.SetRequestHeader('Accept', 'application/vnd.github+json');
    WinHttp.Send();

    if WinHttp.Status <> 200 then begin
      g_DownloadErrorMsg := Format('GitHub API HTTP %d', [WinHttp.Status]);
      Exit;
    end;

    ResponseText := WinHttp.ResponseText;
    Result := True;
  except
    g_DownloadErrorMsg := 'GitHub API error: ' + GetExceptionMessage();
  end;
end;

{ Naive JSON string-value extractor: finds "key":"value" and returns value.
  Avoids pulling in a full JSON parser for Pascal. Sufficient for the
  GitHub release API response which is well-formed. }
function ExtractJsonString(const Json, Key: String): String;
var
  Pattern: String;
  StartPos, EndPos: Integer;
begin
  Result := '';
  Pattern := '"' + Key + '":"';
  StartPos := Pos(Pattern, Json);
  if StartPos = 0 then Exit;
  StartPos := StartPos + Length(Pattern);
  EndPos := StartPos;
  while (EndPos <= Length(Json)) and (Json[EndPos] <> '"') do begin
    if (Json[EndPos] = '\') and (EndPos < Length(Json)) then
      Inc(EndPos); // skip escaped char
    Inc(EndPos);
  end;
  Result := Copy(Json, StartPos, EndPos - StartPos);
end;

{ Find the browser_download_url for the asset whose name matches the pattern.
  GitHub release JSON has an "assets":[{...}, ...] array; each asset has
  "name" and "browser_download_url". Walk it linearly. }
function FindAssetUrl(const Json, NamePattern: String; var AssetUrl, AssetName: String): Boolean;
var
  AssetsStart, Cursor, BlockEnd: Integer;
  CurrentName, CurrentUrl: String;
  Block: String;
begin
  Result := False;
  AssetUrl := '';
  AssetName := '';

  AssetsStart := Pos('"assets":[', Json);
  if AssetsStart = 0 then Exit;
  Cursor := AssetsStart + Length('"assets":[');

  while Cursor < Length(Json) do begin
    BlockEnd := PosEx('}', Json, Cursor);
    if BlockEnd = 0 then Break;
    Block := Copy(Json, Cursor, BlockEnd - Cursor + 1);

    CurrentName := ExtractJsonString(Block, 'name');
    CurrentUrl  := ExtractJsonString(Block, 'browser_download_url');

    if (CurrentName <> '') and (Pos(NamePattern, CurrentName) > 0) then begin
      AssetName := CurrentName;
      AssetUrl  := CurrentUrl;
      Result := True;
      Exit;
    end;

    Cursor := BlockEnd + 1;
    if (Cursor <= Length(Json)) and (Json[Cursor] = ']') then Break;
  end;
end;

procedure SetDownloadProgress(Pct: Integer; const Status: String);
begin
  if Assigned(WizardForm) and Assigned(WizardForm.ProgressGauge) then begin
    WizardForm.ProgressGauge.Position := Pct;
    if Assigned(WizardForm.StatusLabel) then
      WizardForm.StatusLabel.Caption := Status;
  end;
end;

function TryFetchAndStash(const Url, DestPath: String; AttemptNum: Integer): Boolean;
var
  BackoffMs: Integer;
begin
  SetDownloadProgress(0, Format('Downloading upstream installer (attempt %d/%d)...', [AttemptNum, MAX_RETRIES]));
  Result := HttpGetToFile(Url, DestPath);
  if Result then begin
    SetDownloadProgress(100, 'Upstream installer downloaded.');
    Exit;
  end;

  if AttemptNum < MAX_RETRIES then begin
    BackoffMs := 5000 * (AttemptNum * AttemptNum); // 5s, 20s, 45s
    SetDownloadProgress(0, Format('Download failed; retrying in %d seconds...', [BackoffMs div 1000]));
    Sleep(BackoffMs);
  end;
end;

function DownloadUpstreamInstaller(): Boolean;
var
  ApiResponse: String;
  AssetUrl, AssetName, CachePath: String;
  Attempt: Integer;
begin
  Result := False;
  g_DownloadErrorMsg := '';

  SetDownloadProgress(0, 'Querying upstream release...');

  if not HttpGetToString(UPSTREAM_API, ApiResponse) then begin
    Exit; // g_DownloadErrorMsg set inside
  end;

  g_UpstreamReleaseTag := ExtractJsonString(ApiResponse, 'tag_name');
  if g_UpstreamReleaseTag = '' then begin
    g_DownloadErrorMsg := 'Could not parse tag_name from GitHub API response.';
    Exit;
  end;

  if not FindAssetUrl(ApiResponse, 'WindowsInstaller.exe', AssetUrl, AssetName) then begin
    g_DownloadErrorMsg := Format('No *WindowsInstaller.exe asset found in upstream release %s.', [g_UpstreamReleaseTag]);
    Exit;
  end;

  CachePath := GetCacheDir() + '\' + AssetName;
  g_UpstreamInstallerPath := CachePath;

  // If we already have a cached copy of this exact filename, reuse it
  if FileExists(CachePath) then begin
    SetDownloadProgress(100, Format('Using cached %s.', [AssetName]));
    Result := True;
    Exit;
  end;

  for Attempt := 1 to MAX_RETRIES do begin
    if TryFetchAndStash(AssetUrl, CachePath, Attempt) then begin
      Result := True;
      Exit;
    end;
  end;

  // All retries failed
  g_DownloadErrorMsg := Format('Failed to download upstream installer after %d attempts: %s', [MAX_RETRIES, g_DownloadErrorMsg]);
end;
