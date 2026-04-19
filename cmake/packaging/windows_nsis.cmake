# NSIS Packaging
# see options at: https://cmake.org/cmake/help/latest/cpack_gen/nsis.html

set(CPACK_NSIS_INSTALLED_ICON_NAME "${PROJECT__DIR}\\\\${PROJECT_EXE}")

# Due to limitations of CPack's NSIS template, we cannot directly modify the .onInit function
# but we can achieve the same result with the following approach:
# Use MUI_PAGE_CUSTOMFUNCTION_PRE to read the registry before the directory page is shown
#
# Note: CPACK_NSIS_INSTALLER_MUI_ICON_CODE is a hook that runs before page definitions.
# We use it to define custom functions and to set MUI_PAGE_CUSTOMFUNCTION_PRE.

set(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "
; Define the installer icon
!define MUI_ICON \\\"${CMAKE_SOURCE_DIR}/sunshine.ico\\\"
!define MUI_UNICON \\\"${CMAKE_SOURCE_DIR}/sunshine.ico\\\"

; Define the function to run before the directory page is shown
!define MUI_PAGE_CUSTOMFUNCTION_PRE PreDirectoryPage

; Read the previous install path from the registry
; Use a custom registry key so it isn't cleared when an overwrite-install triggers an uninstall

Function PreDirectoryPage
    ; Only try to read the registry if the install dir is still the default
    StrCmp $IS_DEFAULT_INSTALLDIR '1' 0 SkipRegRead

    Push $0
    SetRegView 64

    ; Read the last install directory from our custom registry key
    ReadRegStr $0 HKLM 'SOFTWARE\\\\AlkaidLab\\\\Sunshine' 'InstallDir'
    StrCmp $0 '' DoneRegRead 0
    IfFileExists '$0\\\\*.*' SetPath DoneRegRead

    SetPath:
    StrCpy $INSTDIR $0
    StrCpy $IS_DEFAULT_INSTALLDIR '0'

    DoneRegRead:
    Pop $0

    SkipRegRead:
FunctionEnd

; Helper function: get the parent directory of a path
Function GetParent
    Exch $0
    Push $1
    Push $2

    StrLen $1 $0
    IntOp $1 $1 - 1

    loop:
        IntOp $1 $1 - 1
        IntCmp $1 0 done done
        StrCpy $2 $0 1 $1
        StrCmp $2 '\\\\' found
        Goto loop

    found:
        StrCpy $0 $0 $1

    done:
        Pop $2
        Pop $1
        Exch $0
FunctionEnd

; Custom Finish Page options
; Checkbox 1: Open the documentation (checked by default)
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT 'Open documentation'
!define MUI_FINISHPAGE_RUN_FUNCTION OpenDocumentation

; Checkbox 2: Launch Sunshine GUI (checked by default)
!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_TEXT 'Launch Sunshine GUI'
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION LaunchGUI

Function OpenDocumentation
    ExecShell 'open' 'https://docs.qq.com/aio/DSGdQc3htbFJjSFdO?p=DXpTjzl2kZwBjN7jlRMkRJ'
FunctionEnd

Function LaunchGUI
    Exec '\$INSTDIR\\\\assets\\\\gui\\\\sunshine-gui.exe'
FunctionEnd
")

# ==============================================================================
# File Conflict Prevention - stop processes before files are extracted
# ==============================================================================

# Strategy: simply disable ENABLE_UNINSTALL_BEFORE_INSTALL and handle it manually
# during installation. This avoids file-conflict checks during the directory-selection stage.

# Automatic uninstall feature
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON")

# Windows Restart Manager support and high-DPI bitmap optimizations
set(CPACK_NSIS_EXTRA_DEFINES "
\${CPACK_NSIS_EXTRA_DEFINES}
!define MUI_FINISHPAGE_REBOOTLATER_DEFAULT
ManifestDPIAware true
")

# Basic installer configuration
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}\\\\sunshine.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}\\\\sunshine.ico")

# Set DPI awareness
set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}\\\\welcome.bmp")
set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}\\\\welcome.bmp")

# Header image (must be 150x57 pixels)
# set(CPACK_NSIS_MUI_HEADERIMAGE_BITMAP "${CMAKE_SOURCE_DIR}\\\\cmake\\\\packaging\\\\welcome.bmp")

# Custom branding
set(CPACK_NSIS_BRANDING_TEXT "Sunshine Foundation Game Streaming Server v${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_BRANDING_TEXT_TRIM_POSITION "LEFT")

# ==============================================================================
# Page Customization and Enhanced User Experience
# ==============================================================================

# Custom welcome page text
set(CPACK_NSIS_WELCOME_TITLE "Welcome to Sunshine Foundation Game Streaming Server Install Wizard")
set(CPACK_NSIS_WELCOME_TITLE_3LINES "ON")

# Custom finish page configuration
set(CPACK_NSIS_FINISH_TITLE "Installation complete!")
set(CPACK_NSIS_FINISH_TEXT "Sunshine Foundation Game Streaming Server has been successfully installed on your system.\\r\\n\\r\\nClick 'Finish' to start using this powerful game streaming server.")

# ==============================================================================
# Installation Progress and User Feedback
# ==============================================================================

# Enhanced installation commands with progress feedback
SET(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}
        ; Make sure overwrite mode is still in effect
        SetOverwrite try

        ; ----------------------------------------------------------------------
        ; Clean up portable scripts: the installed version doesn't need these two files
        ; Requirement: if install_portable.bat / uninstall_portable.bat are in the directory, delete them
        ; Safety: prevent symlink attacks - use IfFileExists to check that the file exists
        ;         restrict to within \$INSTDIR to avoid path-traversal attacks
        ; ----------------------------------------------------------------------
        DetailPrint 'Cleaning up portable scripts...'
        ; Safe delete: check that the file exists first to avoid symlink attacks
        IfFileExists '\$INSTDIR\\\\install_portable.bat' 0 +2
        Delete '\$INSTDIR\\\\install_portable.bat'
        IfFileExists '\$INSTDIR\\\\uninstall_portable.bat' 0 +2
        Delete '\$INSTDIR\\\\uninstall_portable.bat'

        ; Reset file permissions
        DetailPrint 'Resetting file permissions...'
        nsExec::ExecToLog 'icacls \\\"$INSTDIR\\\" /reset /T /C /Q >nul 2>&1'

        ; ----------------------------------------------------------------------
        ; Clean up temporary files
        ; Safety: prevent symlink attacks
        ;         Note: wildcard delete (*.tmp, *.old) can be risky against symlinks,
        ;         but it's restricted to within \$INSTDIR, and NSIS's Delete command
        ;         handles symlinks by deleting the link itself rather than the target.
        ;         For more safety, files could be checked one by one, but wildcard
        ;         deletes inside the install directory are low-risk in practice.
        ; ----------------------------------------------------------------------
        DetailPrint 'Cleaning up temporary files...'
        ; Wildcard delete restricted to within \$INSTDIR
        ; NSIS's Delete command removes the symlink itself rather than following it
        Delete '\$INSTDIR\\\\*.tmp'
        Delete '\$INSTDIR\\\\*.old'

        ; Show installation progress info
        DetailPrint 'Configuring Sunshine Foundation Game Streaming Server...'

        ; System configuration
        DetailPrint 'Configuring system permissions...'
        nsExec::ExecToLog 'icacls \\\"$INSTDIR\\\" /reset'

        DetailPrint 'Updating system PATH environment variable...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\update-path.bat\\\" add'

        DetailPrint 'Migrating configuration files...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\migrate-config.bat\\\"'

        DetailPrint 'Configuring firewall rules...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\add-firewall-rule.bat\\\"'

        DetailPrint 'Installing virtual display driver...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\install-vdd.bat\\\"'


        DetailPrint 'Installing virtual gamepad...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\install-gamepad.bat\\\"'

        DetailPrint 'Installing and starting system service...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\install-service.bat\\\"'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\autostart-service.bat\\\"'

        ; Save the install directory so subsequent overwrite installs can read it
        SetRegView 64
        WriteRegStr HKLM 'SOFTWARE\\\\AlkaidLab\\\\Sunshine' 'InstallDir' '$INSTDIR'

        DetailPrint 'Installation complete!'

        NoController:
        ")

# Uninstall command configuration
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}
        ; Show uninstall progress info
        DetailPrint 'Uninstalling Sunshine Foundation Game Streaming Server...'

        ; Stop running programs
        DetailPrint 'Stopping running programs...'
        nsExec::ExecToLog 'taskkill /f /im sunshine-gui.exe'
        nsExec::ExecToLog 'taskkill /f /im sunshine.exe'

        ; Uninstall system components
        DetailPrint 'Removing firewall rules...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\delete-firewall-rule.bat\\\"'

        DetailPrint 'Uninstalling system service...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\uninstall-service.bat\\\"'

        DetailPrint 'Uninstalling virtual display driver...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\uninstall-vdd.bat\\\"'


        DetailPrint 'Restoring NVIDIA settings...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe\\\" --restore-nvprefs-undo'

        MessageBox MB_YESNO|MB_ICONQUESTION \
            'Do you want to remove Virtual Gamepad?' \
            /SD IDNO IDNO NoGamepad
            nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\uninstall-gamepad.bat\\\"'; skipped if no
        NoGamepad:
        MessageBox MB_YESNO|MB_ICONQUESTION \
            'Do you want to remove $INSTDIR (this includes the configuration, cover images, and settings)?' \
            /SD IDNO IDNO NoDelete
            RMDir /r \\\"$INSTDIR\\\"; skipped if no
            SetRegView 64
            DeleteRegValue HKLM 'SOFTWARE\\\\AlkaidLab\\\\Sunshine' 'InstallDir'
            DeleteRegKey /ifempty HKLM 'SOFTWARE\\\\AlkaidLab\\\\Sunshine'

        DetailPrint 'Cleaning up environment variables...'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\update-path.bat\\\" remove'

        NoDelete:
        DetailPrint 'Uninstall complete!'
        ")

# ==============================================================================
# Start Menu and Shortcuts Configuration
# ==============================================================================

set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
set(CPACK_NSIS_INSTALLED_ICON_NAME "${CMAKE_PROJECT_NAME}.exe")

# Enhanced Start Menu shortcuts with better icons and descriptions
set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "${CPACK_NSIS_CREATE_ICONS_EXTRA}
        SetOutPath '\$INSTDIR'

        ; Main program shortcut - uses the executable's embedded icon
        CreateShortCut '\$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Sunshine.lnk' \
            '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' '--shortcut' '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' 0

        ; Install-directory main program shortcut - uses the executable's embedded icon
        CreateShortCut '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.lnk' \
            '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' '--shortcut' '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' 0

        ; GUI management tool shortcut - uses the GUI program's embedded icon
        CreateShortCut '\$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Sunshine GUI.lnk' \
            '\$INSTDIR\\\\assets\\\\gui\\\\sunshine-gui.exe' '' '\$INSTDIR\\\\assets\\\\gui\\\\sunshine-gui.exe' 0

        ; Tools folder shortcut - uses the main program icon
        CreateShortCut '\$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Sunshine Tools.lnk' \
            '\$INSTDIR\\\\tools' '' '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' 0

        ; Create desktop shortcut - uses the executable's embedded icon
        CreateShortCut '\$DESKTOP\\\\Sunshine.lnk' \
            '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' '--shortcut' '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' 0

        ; Create desktop shortcut - GUI management tool
        CreateShortCut '\$DESKTOP\\\\Sunshine GUI.lnk' \
            '\$INSTDIR\\\\assets\\\\gui\\\\sunshine-gui.exe' '' '\$INSTDIR\\\\assets\\\\gui\\\\sunshine-gui.exe' 0
        ")

set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "${CPACK_NSIS_DELETE_ICONS_EXTRA}
        ; ----------------------------------------------------------------------
        ; Safe shortcut deletion: prevent symlink attacks and path traversal
        ;
        ; Security analysis:
        ; 1. Symlink attack risk: if an attacker creates a symlink in the desktop/start
        ;    menu using one of our shortcut names, deleting it could mistakenly remove
        ;    other files. However, NSIS's Delete command removes the symlink itself
        ;    rather than following it.
        ; 2. Path-traversal risk: we use fixed system variables (\$DESKTOP, \$SMPROGRAMS)
        ;    and accept no external input; the paths are hardcoded, which reduces the
        ;    risk of path traversal.
        ; 3. File-type validation: we only delete the expected .lnk files, and their
        ;    names are fixed, which reduces the risk of accidental deletion.
        ;
        ; Protective measures:
        ; - Use IfFileExists to check that the file exists, to avoid deleting non-existent files
        ; - Use fixed system path variables and accept no external input
        ; - Only delete expected .lnk files with hardcoded filenames
        ; - NSIS's Delete command automatically handles symlinks, only removing the link itself
        ; ----------------------------------------------------------------------

        ; Delete Start Menu shortcuts (safe delete)
        ; Note: \$MUI_TEMP is an NSIS internal variable pointing to the Start Menu folder, controlled by the installer
        IfFileExists '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine.lnk' 0 +2
        Delete '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine.lnk'
        IfFileExists '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine GUI.lnk' 0 +2
        Delete '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine GUI.lnk'
        IfFileExists '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine Tools.lnk' 0 +2
        Delete '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine Tools.lnk'
        IfFileExists '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine Service.lnk' 0 +2
        Delete '\$SMPROGRAMS\\\\$MUI_TEMP\\\\Sunshine Service.lnk'
        IfFileExists '\$SMPROGRAMS\\\\$MUI_TEMP\\\\${CMAKE_PROJECT_NAME}.lnk' 0 +2
        Delete '\$SMPROGRAMS\\\\$MUI_TEMP\\\\${CMAKE_PROJECT_NAME}.lnk'

        ; Delete desktop shortcuts (safe delete)
        ; Note: \$DESKTOP is an NSIS system variable pointing to the current user's desktop directory.
        ;       If an attacker creates a symlink, NSIS's Delete removes the link itself rather than following it.
        IfFileExists '\$DESKTOP\\\\Sunshine.lnk' 0 +2
        Delete '\$DESKTOP\\\\Sunshine.lnk'
        IfFileExists '\$DESKTOP\\\\Sunshine GUI.lnk' 0 +2
        Delete '\$DESKTOP\\\\Sunshine GUI.lnk'
        ")

# ==============================================================================
# Advanced Installation Features
# ==============================================================================

# Custom installation options
set(CPACK_NSIS_COMPRESSOR "lzma")  # Better compression
set(CPACK_NSIS_COMPRESSOR_OPTIONS "/SOLID")  # Solid compression for smaller file size

# ==============================================================================
# Support Links and Documentation
# ==============================================================================

set(CPACK_NSIS_HELP_LINK "https://alkaidlab.com/sunshine/docs")
set(CPACK_NSIS_URL_INFO_ABOUT "${CMAKE_PROJECT_HOMEPAGE_URL}")
set(CPACK_NSIS_CONTACT "${CMAKE_PROJECT_HOMEPAGE_URL}/support")

# ==============================================================================
# System Integration and Compatibility
# ==============================================================================

# Enable high DPI awareness for modern displays
set(CPACK_NSIS_MANIFEST_DPI_AWARE true)

# Request administrator privileges for proper installation
set(CPACK_NSIS_REQUEST_EXECUTION_LEVEL "admin")

# Custom installer appearance
set(CPACK_NSIS_DISPLAY_NAME "Sunshine Foundation Game Streaming Server v${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_PACKAGE_NAME "Sunshine")
