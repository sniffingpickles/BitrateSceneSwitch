; ---------------------------------------------------------------------------
; Bitrate Scene Switch - NSIS Installer
;
; Installs the plugin into the system-wide OBS Studio plugin directory.
; Auto-detects OBS via the Windows registry; falls back to manual browse.
; Creates a standard Add/Remove Programs entry with uninstaller.
; ---------------------------------------------------------------------------

!include "MUI2.nsh"
!include "FileFunc.nsh"

; ---------------------------------------------------------------------------
; Build-time defines (overridden by CI via /D flags)
; ---------------------------------------------------------------------------
!ifndef PLUGIN_VERSION
    !define PLUGIN_VERSION "0.0.0"
!endif

!define PLUGIN_NAME      "BitrateSceneSwitch"
!define PUBLISHER        "sniffingpickles"
!define WEBSITE          "https://github.com/sniffingpickles/BitrateSceneSwitch"
!define UNINSTALL_REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PLUGIN_NAME}"

Name        "${PLUGIN_NAME} ${PLUGIN_VERSION}"
OutFile     "${PLUGIN_NAME}-${PLUGIN_VERSION}-windows-x64-setup.exe"
InstallDir  ""
Unicode     true

RequestExecutionLevel admin

; ---------------------------------------------------------------------------
; Modern UI pages
; ---------------------------------------------------------------------------
!define MUI_ABORTWARNING
!define MUI_ICON   "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------------------
; Detect OBS Studio install path from the registry
; ---------------------------------------------------------------------------
Function .onInit
    ; Try the 64-bit uninstall key first (standard OBS installer)
    ReadRegStr $0 HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
        "InstallLocation"
    StrCmp $0 "" 0 found

    ; Fallback: HKLM 32-bit view
    SetRegView 32
    ReadRegStr $0 HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
        "InstallLocation"
    SetRegView lastused
    StrCmp $0 "" 0 found

    ; Fallback: common default path
    StrCpy $0 "$PROGRAMFILES64\obs-studio"
    IfFileExists "$0\bin\64bit\obs64.exe" found 0

    ; Give up on auto-detect; let the user browse
    StrCpy $INSTDIR "$PROGRAMFILES64\obs-studio"
    Return

found:
    StrCpy $INSTDIR $0
FunctionEnd

; ---------------------------------------------------------------------------
; Install section
; ---------------------------------------------------------------------------
Section "Install"
    SetOutPath "$INSTDIR\obs-plugins\64bit"
    File "release\obs-plugins\64bit\${PLUGIN_NAME}.dll"
    File "release\obs-plugins\64bit\libcurl-x64.dll"
    File /nonfatal "release\obs-plugins\64bit\Qt6WebSockets.dll"

    SetOutPath "$INSTDIR\data\obs-plugins\${PLUGIN_NAME}"
    File /r "release\data\obs-plugins\${PLUGIN_NAME}\*.*"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\obs-plugins\64bit\${PLUGIN_NAME}-uninstall.exe"

    ; Add/Remove Programs entry
    WriteRegStr HKLM "${UNINSTALL_REGKEY}" "DisplayName"     "${PLUGIN_NAME}"
    WriteRegStr HKLM "${UNINSTALL_REGKEY}" "DisplayVersion"  "${PLUGIN_VERSION}"
    WriteRegStr HKLM "${UNINSTALL_REGKEY}" "Publisher"        "${PUBLISHER}"
    WriteRegStr HKLM "${UNINSTALL_REGKEY}" "URLInfoAbout"     "${WEBSITE}"
    WriteRegStr HKLM "${UNINSTALL_REGKEY}" "UninstallString"  '"$INSTDIR\obs-plugins\64bit\${PLUGIN_NAME}-uninstall.exe"'
    WriteRegDWORD HKLM "${UNINSTALL_REGKEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_REGKEY}" "NoRepair" 1

    ; Compute and store installed size (kilobytes)
    ${GetSize} "$INSTDIR\obs-plugins\64bit" "/M=${PLUGIN_NAME}.dll /S=0K /G=0" $0 $1 $2
    IntOp $0 $0 + 0
    WriteRegDWORD HKLM "${UNINSTALL_REGKEY}" "EstimatedSize" $0
SectionEnd

; ---------------------------------------------------------------------------
; Uninstall section
; ---------------------------------------------------------------------------
Section "Uninstall"
    Delete "$INSTDIR\obs-plugins\64bit\${PLUGIN_NAME}.dll"
    Delete "$INSTDIR\obs-plugins\64bit\libcurl-x64.dll"
    Delete "$INSTDIR\obs-plugins\64bit\Qt6WebSockets.dll"
    ; clean up leftovers from earlier builds
    Delete "$INSTDIR\bin\64bit\Qt6WebSockets.dll"
    Delete "$INSTDIR\obs-plugins\64bit\Qt6Network.dll"
    Delete "$INSTDIR\obs-plugins\64bit\${PLUGIN_NAME}-uninstall.exe"
    RMDir /r "$INSTDIR\data\obs-plugins\${PLUGIN_NAME}"

    DeleteRegKey HKLM "${UNINSTALL_REGKEY}"
SectionEnd
