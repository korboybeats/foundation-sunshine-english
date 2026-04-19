# `run_command` Function Developer Guide

## 1. Function Signature

```cpp
bp::child run_command(
    bool elevated,                      // run with elevated privileges
    bool interactive,                   // interactive process (needs a console window)
    const std::string &cmd,             // command string to execute
    boost::filesystem::path &working_dir, // working directory
    const bp::environment &env,         // environment variables (includes SUNSHINE_* vars)
    FILE *file,                         // output redirection file (may be nullptr)
    std::error_code &ec,                // error code output
    bp::group *group                    // process group / job object (may be nullptr)
);
```

## 2. Overview

`run_command` is the **unified process-launch interface** on the Windows platform:

- **Cross-privilege execution**: a SYSTEM service can launch a process as the current user
- **Smart command parsing**: automatically handles URLs, file associations, and executables
- **Environment variable expansion**: supports variables like `%SUNSHINE_CLIENT_WIDTH%`
- **Job object management**: supports process lifecycle tracking

## 3. Execution Flow

```
run_command entry
      │
      ▼
1. Initialize: create STARTUPINFOEXW, clone the environment
      │
      ▼
2. Expand environment variables: expand_env_vars_in_cmd(cmd, cloned_env)
   - %SUNSHINE_CLIENT_WIDTH% → 1920
   - %SUNSHINE_CLIENT_HEIGHT% → 1080
      │
      ▼
3. Resolve the command: resolve_command_string()
   - URL → look up the default browser in the registry
   - .exe → execute directly
   - other files → look up the associated program
      │
      ▼
4. Process creation
   - SYSTEM mode: CreateProcessAsUserW (impersonate the user)
   - Normal mode: CreateProcessW
      │
      ▼
5. Return the bp::child object
```

## 4. Environment Variable Expansion

### 4.1 Built-in Sunshine environment variables

Set in `process.cpp` and `nvhttp.cpp`, stored in `_env`. They can be referenced in commands via `%VAR%` syntax.

#### Application variables

| Name | Type | Description | Example |
|--------|------|------|--------|
| `SUNSHINE_APP_ID` | number | Application ID | `"123"` |
| `SUNSHINE_APP_NAME` | string | Application name | `"Game"` |

#### Client identification variables

| Name | Type | Description | Example |
|--------|------|------|--------|
| `SUNSHINE_CLIENT_ID` | number | Client session ID | `"12345"` |
| `SUNSHINE_CLIENT_UNIQUE_ID` | string | Unique client identifier | `"unique-id-string"` |
| `SUNSHINE_CLIENT_NAME` | string | Client device name | `"iPhone"` |

#### Client display variables

| Name | Type | Description | Example |
|--------|------|------|--------|
| `SUNSHINE_CLIENT_WIDTH` | number | Client screen width (pixels) | `"1920"` |
| `SUNSHINE_CLIENT_HEIGHT` | number | Client screen height (pixels) | `"1080"` |
| `SUNSHINE_CLIENT_FPS` | number | Client refresh rate (FPS) | `"60"` |
| `SUNSHINE_CLIENT_HDR` | bool | Whether HDR is enabled | `"true"` or `"false"` |
| `SUNSHINE_CLIENT_CUSTOM_SCREEN_MODE` | number | Custom screen mode | `"0"` |

#### Client audio variables

| Name | Type | Description | Example |
|--------|------|------|--------|
| `SUNSHINE_CLIENT_AUDIO_CONFIGURATION` | string | Audio configuration (only set when supported) | `"2.0"`, `"5.1"`, `"7.1"` |
| `SUNSHINE_CLIENT_AUDIO_SURROUND_PARAMS` | string | Surround sound parameters | depends on the client |

#### Client feature variables

| Name | Type | Description | Example |
|--------|------|------|--------|
| `SUNSHINE_CLIENT_GCMAP` | number | Game controller mapping | `"0"` |
| `SUNSHINE_CLIENT_HOST_AUDIO` | bool | Whether to use host audio | `"true"` or `"false"` |
| `SUNSHINE_CLIENT_ENABLE_SOPS` | bool | Whether SOPS is enabled | `"true"` or `"false"` |
| `SUNSHINE_CLIENT_ENABLE_MIC` | bool | Whether the microphone is enabled | `"true"` or `"false"` |
| `SUNSHINE_CLIENT_USE_VDD` | bool | Whether to use the virtual display | `"true"` or `"false"` |
| `SUNSHINE_CLIENT_CERT_UUID` | string | Client certificate UUID (stable client identifier; only set when present) | `"uuid-string"` |

#### Usage examples

```bash
# Use the resolution variables
qres.exe /x:%SUNSHINE_CLIENT_WIDTH% /y:%SUNSHINE_CLIENT_HEIGHT% /r:%SUNSHINE_CLIENT_FPS%

# Use the HDR state
if "%SUNSHINE_CLIENT_HDR%"=="true" (
    enable_hdr.exe
)

# Use the client name
echo "Connected from: %SUNSHINE_CLIENT_NAME%"

# Use the microphone state
if "%SUNSHINE_CLIENT_ENABLE_MIC%"=="true" (
    configure_mic.exe
)
```

### 4.2 Expansion implementation

Done by `expand_env_vars_in_cmd` (in `misc.cpp`):

```cpp
std::string expand_env_vars_in_cmd(const std::string &cmd, const bp::environment &env) {
    // 1. Fast path: if there is no '%', return immediately
    // 2. Walk char-by-char to parse %VAR% patterns
    // 3. First look up in cloned_env (which contains the SUNSHINE_* vars)
    // 4. Then look up in the system environment
    // 5. If not found, leave the original text alone
}
```

### 4.3 Why not use `ExpandEnvironmentStringsW`?

- `ExpandEnvironmentStringsW` can only access the **current process's environment**.
- Variables like `SUNSHINE_CLIENT_WIDTH` live in `cloned_env`, not in the Sunshine process environment.
- So we have to look them up and substitute them by hand from `cloned_env`.

### 4.4 Variable lookup order

1. First look in `cloned_env` (case-insensitive)
2. Then look in the system environment (via `GetEnvironmentVariableA`)
3. If still not found, keep the original `%VAR%` text as-is

### 4.5 Usage examples

```bash
# Use environment variables directly (✅ now supported)
unlocker.exe -screen-width %SUNSHINE_CLIENT_WIDTH% -screen-height %SUNSHINE_CLIENT_HEIGHT%

# After expansion
unlocker.exe -screen-width 1920 -screen-height 1080

# Existing cmd /C usage still works (backward compatible)
cmd /C qres.exe /x:%SUNSHINE_CLIENT_WIDTH% /y:%SUNSHINE_CLIENT_HEIGHT%
```

### 4.6 Special cases

| Input | Output | Notes |
|------|------|------|
| `%SUNSHINE_CLIENT_WIDTH%` | `1920` | Normal expansion |
| `%UNKNOWN%` | `%UNKNOWN%` | Not found; kept as-is |
| `%%` | `%` | Escape sequence |
| `100%` | `100%` | Unpaired `%`; kept as-is |

## 5. URL Handling

When the command is a URL, the registry is queried for the default browser:

```
Input: "https://github.com/example"
  │
  ▼
PathIsURLW() → is a URL
  │
  ▼
Extract scheme: "https"
  │
  ▼
AssocQueryStringW() queries the registry
  │
  ▼
Get the default browser command: "C:\...\chrome.exe" "%1"
  │
  ▼
Substitute %1: "C:\...\chrome.exe" "https://github.com/example"
```

## 6. Notes

1. **Environment isolation**: the `env` you pass in is cloned, so it does not leak into other callers.
2. **Temporary PATH change**: the working directory is temporarily prepended to PATH.
3. **SYSTEM mode**: when running as a service, GUI processes are launched by impersonating the user.
4. **Backward compatibility**: existing `cmd /C` configurations still work.

---

*Last updated: 2024*
