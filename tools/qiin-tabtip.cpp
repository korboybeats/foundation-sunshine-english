/**
 * @file tools/qiin-tabtip.cpp
 * @brief Utility to show or hide the Windows touch virtual keyboard
 * @note Optimized version - avoids the C++ standard library to reduce file size
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <initguid.h>
#include <Objbase.h>

// Simple console output helper (replaces iostream)
static void Print(const wchar_t* msg) {
  DWORD written;
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  WriteConsoleW(hConsole, msg, lstrlenW(msg), &written, NULL);
  WriteConsoleW(hConsole, L"\r\n", 2, &written, NULL);
}

static void PrintError(const wchar_t* msg) {
  DWORD written;
  HANDLE hConsole = GetStdHandle(STD_ERROR_HANDLE);
  WriteConsoleW(hConsole, msg, lstrlenW(msg), &written, NULL);
  WriteConsoleW(hConsole, L"\r\n", 2, &written, NULL);
}

// Case-insensitive string comparison
static bool StrEqualI(const wchar_t* str1, const wchar_t* str2) {
  return lstrcmpiW(str1, str2) == 0;
}

// ITipInvocation COM interface - this is Microsoft's official touch keyboard API
// CLSID for UIHostNoLaunch
DEFINE_GUID(CLSID_UIHostNoLaunch,
    0x4CE576FA, 0x83DC, 0x4f88, 0x95, 0x1C, 0x9D, 0x07, 0x82, 0xB4, 0xE3, 0x76);

// IID for ITipInvocation
DEFINE_GUID(IID_ITipInvocation,
    0x37c994e7, 0x432b, 0x4834, 0xa2, 0xf7, 0xdc, 0xe1, 0xf1, 0x3b, 0x83, 0x4b);

// ITipInvocation interface definition
struct ITipInvocation : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Toggle(HWND wnd) = 0;
};

// Path to the Windows 10/11 touch keyboard
const wchar_t* TABTIP_PATH = L"C:\\Program Files\\Common Files\\microsoft shared\\ink\\TabTip.exe";

/**
 * Check whether the touch keyboard is currently running
 */
bool IsKeyboardVisible() {
  HWND hwnd = FindWindow(L"IPTip_Main_Window", NULL);
  if (hwnd == NULL) {
    // Windows 11 may use a different class name
    hwnd = FindWindow(L"ApplicationFrameWindow", L"Microsoft Text Input Application");
  }

  if (hwnd != NULL) {
    // Check whether the window is visible
    return IsWindowVisible(hwnd);
  }
  return false;
}

/**
 * Check whether TabTip.exe exists
 */
bool CheckTabTipExists() {
  DWORD dwAttrib = GetFileAttributes(TABTIP_PATH);
  return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * Enable desktop-mode auto-invoke for the touch keyboard
 * This is the key setting for showing TabTip on Windows 10/11
 */
bool EnableDesktopModeAutoInvoke() {
  HKEY hKey;
  LONG result = RegOpenKeyEx(
    HKEY_CURRENT_USER,
    L"SOFTWARE\\Microsoft\\TabletTip\\1.7",
    0,
    KEY_READ | KEY_WRITE,
    &hKey
  );

  if (result != ERROR_SUCCESS) {
    // If the key does not exist, try to create it
    result = RegCreateKeyEx(
      HKEY_CURRENT_USER,
      L"SOFTWARE\\Microsoft\\TabletTip\\1.7",
      0,
      NULL,
      REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE,
      NULL,
      &hKey,
      NULL
    );

    if (result != ERROR_SUCCESS) {
      return false;
    }
  }

  // Set EnableDesktopModeAutoInvoke to 1
  DWORD value = 1;
  result = RegSetValueEx(
    hKey,
    L"EnableDesktopModeAutoInvoke",
    0,
    REG_DWORD,
    (BYTE*)&value,
    sizeof(DWORD)
  );

  RegCloseKey(hKey);
  return result == ERROR_SUCCESS;
}

/**
 * Check whether desktop-mode auto-invoke is enabled
 */
bool IsDesktopModeAutoInvokeEnabled() {
  HKEY hKey;
  LONG result = RegOpenKeyEx(
    HKEY_CURRENT_USER,
    L"SOFTWARE\\Microsoft\\TabletTip\\1.7",
    0,
    KEY_READ,
    &hKey
  );

  if (result != ERROR_SUCCESS) {
    return false;
  }

  DWORD value = 0;
  DWORD size = sizeof(DWORD);
  result = RegQueryValueEx(
    hKey,
    L"EnableDesktopModeAutoInvoke",
    NULL,
    NULL,
    (BYTE*)&value,
    &size
  );

  RegCloseKey(hKey);
  return (result == ERROR_SUCCESS && value == 1);
}

/**
 * Force-show an existing keyboard window
 */
bool ForceShowKeyboardWindow() {
  // Find the keyboard window
  HWND hwnd = FindWindow(L"IPTip_Main_Window", NULL);

  if (hwnd == NULL) {
    // Windows 11 may use a different class name
    hwnd = FindWindow(L"ApplicationFrameWindow", L"Microsoft Text Input Application");
  }

  if (hwnd != NULL) {
    // Show the window
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    // Make sure the window is on screen
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int keyboardHeight = rect.bottom - rect.top;

    if (keyboardHeight > 0) {
      int screenWidth = GetSystemMetrics(SM_CXSCREEN);
      int screenHeight = GetSystemMetrics(SM_CYSCREEN);
      int keyboardWidth = rect.right - rect.left;
      int x = (screenWidth - keyboardWidth) / 2;
      int y = screenHeight - keyboardHeight - 50;

      SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
      Sleep(50);
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }

    return IsWindowVisible(hwnd);
  }

  return false;
}

/**
 * Show the touch keyboard via the COM interface (recommended method)
 * This uses Microsoft's official ITipInvocation interface
 */
bool ShowKeyboardViaCOM() {
  HRESULT hr = CoInitialize(NULL);
  bool needsUninit = SUCCEEDED(hr);

  ITipInvocation* pTipInvocation = NULL;
  hr = CoCreateInstance(
    CLSID_UIHostNoLaunch,
    NULL,
    CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER,
    IID_ITipInvocation,
    (void**)&pTipInvocation
  );

  bool success = false;
  if (SUCCEEDED(hr) && pTipInvocation) {
    hr = pTipInvocation->Toggle(GetDesktopWindow());
    success = SUCCEEDED(hr);
    pTipInvocation->Release();
  }

  if (needsUninit) {
    CoUninitialize();
  }

  return success;
}

/**
 * Show the touch keyboard (combined methods)
 */
bool ShowKeyboard() {
  // Method 1: COM interface (most reliable)
  if (ShowKeyboardViaCOM()) {
    Print(L"Touch keyboard shown");
    return true;
  }

  // Method 2: legacy fallback
  if (!CheckTabTipExists()) {
    PrintError(L"TabTip.exe not found");
    return false;
  }

  // Make sure the registry setting is correct
  if (!IsDesktopModeAutoInvokeEnabled()) {
    EnableDesktopModeAutoInvoke();
  }

  // Check whether the window already exists
  HWND existingWnd = FindWindow(L"IPTip_Main_Window", NULL);
  if (existingWnd == NULL) {
    existingWnd = FindWindow(L"ApplicationFrameWindow", L"Microsoft Text Input Application");
  }

  if (existingWnd != NULL) {
    if (ForceShowKeyboardWindow()) {
      Print(L"Touch keyboard shown");
      return true;
    }
  }

  // Launch TabTip.exe
  SHELLEXECUTEINFO sei = { 0 };
  sei.cbSize = sizeof(SHELLEXECUTEINFO);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
  sei.lpVerb = L"open";
  sei.lpFile = TABTIP_PATH;
  sei.nShow = SW_SHOW;

  if (ShellExecuteEx(&sei)) {
    if (sei.hProcess) {
      WaitForSingleObject(sei.hProcess, 1000);
      CloseHandle(sei.hProcess);
    }
    Sleep(500);

    if (ForceShowKeyboardWindow()) {
      Print(L"Touch keyboard shown");
      return true;
    }
  }

  // Final fallback: OSK
  HINSTANCE result = ShellExecute(NULL, L"open", L"osk.exe", NULL, NULL, SW_SHOW);
  if ((INT_PTR)result > 32) {
    Print(L"On-screen keyboard shown");
    return true;
  }

  PrintError(L"Unable to show keyboard");
  return false;
}

/**
 * Hide the touch keyboard
 */
bool HideKeyboard() {
  // Find the keyboard window
  HWND hwnd = FindWindow(L"IPTip_Main_Window", NULL);
  if (hwnd == NULL) {
    // Windows 11 may use a different class name
    hwnd = FindWindow(L"ApplicationFrameWindow", L"Microsoft Text Input Application");
  }

  if (hwnd != NULL && IsWindowVisible(hwnd)) {
    PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
    Print(L"Touch keyboard hidden");
    return true;
  }

  Print(L"Touch keyboard is not running or already hidden");
  return false;
}

/**
 * Toggle the touch keyboard
 */
bool ToggleKeyboard() {
  if (IsKeyboardVisible()) {
    return HideKeyboard();
  } else {
    return ShowKeyboard();
  }
}

/**
 * Diagnose the system environment
 */
void Diagnose() {
  wchar_t buffer[256];

  Print(L"=== System Diagnostics ===");
  Print(L"");

  // Check Windows version
  OSVERSIONINFOEX osvi = { 0 };
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  #if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable: 4996)
  #endif
  GetVersionEx((LPOSVERSIONINFO)&osvi);
  #if defined(_MSC_VER)
  #pragma warning(pop)
  #endif
  wsprintfW(buffer, L"Windows version: %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
  Print(buffer);

  // Check TabTip.exe
  Print(L"");
  wsprintfW(buffer, L"TabTip path: %s", TABTIP_PATH);
  Print(buffer);
  Print(CheckTabTipExists() ? L"TabTip.exe: present" : L"TabTip.exe: missing");

  // Check the registry setting
  Print(L"");
  Print(L"Registry setting:");
  Print(IsDesktopModeAutoInvokeEnabled() ?
        L"  EnableDesktopModeAutoInvoke: enabled" :
        L"  EnableDesktopModeAutoInvoke: disabled");

  // Check the keyboard window
  Print(L"");
  Print(L"Keyboard window check:");
  HWND hwnd = FindWindow(L"IPTip_Main_Window", NULL);
  if (hwnd) {
    Print(L"  IPTip_Main_Window: found (Windows 10)");
    Print(IsWindowVisible(hwnd) ? L"  Visibility: visible" : L"  Visibility: hidden");
  } else {
    Print(L"  IPTip_Main_Window: not found");
  }

  hwnd = FindWindow(L"ApplicationFrameWindow", L"Microsoft Text Input Application");
  if (hwnd) {
    Print(L"  ApplicationFrameWindow: found (Windows 11)");
    Print(IsWindowVisible(hwnd) ? L"  Visibility: visible" : L"  Visibility: hidden");
  } else {
    Print(L"  ApplicationFrameWindow: not found");
  }

  Print(L"");
  Print(IsKeyboardVisible() ? L"Current keyboard state: visible" : L"Current keyboard state: hidden");
}

/**
 * Show the on-screen keyboard (OSK)
 */
bool ShowOSK() {
  HINSTANCE result = ShellExecute(NULL, L"open", L"osk.exe", NULL, NULL, SW_SHOW);
  if ((INT_PTR)result > 32) {
    Print(L"On-screen keyboard shown");
    return true;
  }
  PrintError(L"Unable to show the on-screen keyboard");
  return false;
}

/**
 * Show usage help
 */
void ShowHelp() {
  Print(L"Windows touch virtual keyboard utility");
  Print(L"");
  Print(L"Usage:");
  Print(L"  qiin-tabtip [option]");
  Print(L"");
  Print(L"Options:");
  Print(L"  show      - Show the touch keyboard (TabTip)");
  Print(L"  hide      - Hide the touch keyboard");
  Print(L"  toggle    - Toggle the keyboard state (default)");
  Print(L"  osk       - Show the on-screen keyboard (OSK)");
  Print(L"  status    - Check whether the keyboard is visible");
  Print(L"  diagnose  - Diagnose the system environment");
  Print(L"  help      - Show this help message");
  Print(L"");
  Print(L"Examples:");
  Print(L"  qiin-tabtip              # Toggle the keyboard state");
  Print(L"  qiin-tabtip show         # Show the touch keyboard");
  Print(L"  qiin-tabtip osk          # Show the on-screen keyboard");
  Print(L"  qiin-tabtip diagnose     # Diagnose problems");
}

int wmain(int argc, wchar_t* argv[]) {
  // Set console output to UTF-8
  SetConsoleOutputCP(CP_UTF8);

  const wchar_t* command = L"toggle";
  wchar_t cmdLower[256] = {0};

  if (argc > 1) {
    command = argv[1];
    // Convert to lowercase for comparison
    lstrcpynW(cmdLower, command, 255);
    CharLowerW(cmdLower);
    command = cmdLower;
  }

  if (StrEqualI(command, L"show")) {
    return ShowKeyboard() ? 0 : 1;
  }
  else if (StrEqualI(command, L"hide")) {
    return HideKeyboard() ? 0 : 1;
  }
  else if (StrEqualI(command, L"toggle")) {
    return ToggleKeyboard() ? 0 : 1;
  }
  else if (StrEqualI(command, L"osk")) {
    return ShowOSK() ? 0 : 1;
  }
  else if (StrEqualI(command, L"status")) {
    if (IsKeyboardVisible()) {
      Print(L"Touch keyboard currently: visible");
      return 0;
    } else {
      Print(L"Touch keyboard currently: hidden");
      return 1;
    }
  }
  else if (StrEqualI(command, L"diagnose") || StrEqualI(command, L"diag")) {
    Diagnose();
    return 0;
  }
  else if (StrEqualI(command, L"help") || StrEqualI(command, L"--help") ||
           StrEqualI(command, L"-h") || StrEqualI(command, L"/?")) {
    ShowHelp();
    return 0;
  }
  else {
    wchar_t errMsg[512];
    wsprintfW(errMsg, L"Unknown command: %s", command);
    PrintError(errMsg);
    PrintError(L"Use 'qiin-tabtip help' to see usage");
    return 1;
  }

  return 0;
}

// If wmain support is not available, fall back to a normal main function
#ifndef _UNICODE
int main(int argc, char* argv[]) {
  // Get the wide-character command line
  LPWSTR* szArglist;
  int nArgs;

  szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
  if (szArglist == NULL) {
    PrintError(L"CommandLineToArgvW failed");
    return 1;
  }

  int result = wmain(nArgs, szArglist);

  LocalFree(szArglist);
  return result;
}
#endif

