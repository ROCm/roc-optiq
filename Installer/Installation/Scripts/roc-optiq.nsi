; Copyright Advanced Micro Devices, Inc.
; SPDX-License-Identifier: MIT
;
; ROCm Optiq Windows Installer (NSIS)
; Usage: makensis roc-optiq.nsi
; Or via CMake: cmake --build <build-dir> --target package-nsis

Unicode True

;-----------------------------------------------------------------------------
; Version info — injected by CMake configure_file or passed via /D on CLI
;-----------------------------------------------------------------------------
!ifndef PRODUCT_VERSION_MAJOR
  !define PRODUCT_VERSION_MAJOR "0"
!endif
!ifndef PRODUCT_VERSION_MINOR
  !define PRODUCT_VERSION_MINOR "4"
!endif
!ifndef PRODUCT_VERSION_PATCH
  !define PRODUCT_VERSION_PATCH "0"
!endif
!ifndef PRODUCT_VERSION_BUILD
  !define PRODUCT_VERSION_BUILD "3"
!endif

!define PRODUCT_VERSION "${PRODUCT_VERSION_MAJOR}.${PRODUCT_VERSION_MINOR}.${PRODUCT_VERSION_PATCH}.${PRODUCT_VERSION_BUILD}"
!define PRODUCT_SHORT_VERSION "${PRODUCT_VERSION_MAJOR}.${PRODUCT_VERSION_MINOR}.${PRODUCT_VERSION_PATCH}"

;-----------------------------------------------------------------------------
; Paths — injected by CMake; fall back to sibling layout for manual builds
;-----------------------------------------------------------------------------
; BUILD_BIN_DIR  : directory containing roc-optiq.exe (and any runtime DLLs)
; SOURCE_DIR     : repository root (for LICENSE.md, resources\ROCm_Optiq.ico)
; OUTPUT_DIR     : where to write the finished installer .exe
!ifndef BUILD_BIN_DIR
  !define BUILD_BIN_DIR "..\..\..\build\x64-release\Release"
!endif
!ifndef SOURCE_DIR
  !define SOURCE_DIR "..\..\..\"
!endif
!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "..\..\..\"
!endif

;-----------------------------------------------------------------------------
; Product metadata
;-----------------------------------------------------------------------------
!define PRODUCT_NAME      "ROCm Optiq"
!define PRODUCT_PUBLISHER "Advanced Micro Devices, Inc."
!define PRODUCT_URL       "https://github.com/ROCm/roc-optiq"
!define PRODUCT_EXE       "roc-optiq.exe"
!define INSTALL_DIR_KEY   "ROCm Optiq"  ; sub-key under $PROGRAMFILES64

; Registry paths used for uninstall entry and upgrade detection
!define UNINSTALL_REG  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define INSTALL_REG    "Software\${PRODUCT_PUBLISHER}\${PRODUCT_NAME}"

;-----------------------------------------------------------------------------
; NSIS includes
;-----------------------------------------------------------------------------
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

;-----------------------------------------------------------------------------
; General settings
;-----------------------------------------------------------------------------
Name          "${PRODUCT_NAME} ${PRODUCT_SHORT_VERSION}"
OutFile       "${OUTPUT_DIR}\roc-optiq-${PRODUCT_VERSION}-windows-x64-setup.exe"
InstallDir    "$PROGRAMFILES64\${INSTALL_DIR_KEY}"
InstallDirRegKey HKLM "${INSTALL_REG}" "InstallDir"
RequestExecutionLevel admin
SetCompressor  /SOLID lzma
SetDatablockOptimize on
CRCCheck on
ShowInstDetails show
ShowUnInstDetails show

;-----------------------------------------------------------------------------
; Version info embedded into the installer .exe itself
;-----------------------------------------------------------------------------
VIProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey "ProductName"     "${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion"  "${PRODUCT_VERSION}"
VIAddVersionKey "CompanyName"     "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright"  "Copyright (C) 2025 ${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"
VIAddVersionKey "FileVersion"     "${PRODUCT_VERSION}"

;-----------------------------------------------------------------------------
; MUI appearance
;-----------------------------------------------------------------------------
!define MUI_ICON   "${SOURCE_DIR}\resources\ROCm_Optiq.ico"
!define MUI_UNICON "${SOURCE_DIR}\resources\ROCm_Optiq.ico"

!ifdef INSTALLER_BANNER_BMP
  !define MUI_WELCOMEFINISHPAGE_BITMAP "${INSTALLER_BANNER_BMP}"
!endif
!ifdef INSTALLER_HEADER_BMP
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP "${INSTALLER_HEADER_BMP}"
!endif

!define MUI_WELCOMEPAGE_TITLE       "Welcome to the ${PRODUCT_NAME} Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT        "This wizard will install ${PRODUCT_NAME} ${PRODUCT_SHORT_VERSION} on your computer.$\r$\n$\r$\nClick Next to continue."
!define MUI_LICENSEPAGE_CHECKBOX
!define MUI_LICENSEPAGE_CHECKBOX_TEXT "I accept the terms of the license agreement"
!define MUI_FINISHPAGE_RUN          "$INSTDIR\${PRODUCT_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT     "Launch ${PRODUCT_NAME}"
!define MUI_FINISHPAGE_LINK         "Visit the ROCm Optiq GitHub page"
!define MUI_FINISHPAGE_LINK_LOCATION "${PRODUCT_URL}"
!define MUI_ABORTWARNING

;-----------------------------------------------------------------------------
; Installer pages
;-----------------------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_DIR}\LICENSE.md"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

;-----------------------------------------------------------------------------
; Uninstaller pages
;-----------------------------------------------------------------------------
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;-----------------------------------------------------------------------------
; Language
;-----------------------------------------------------------------------------
!insertmacro MUI_LANGUAGE "English"

;=============================================================================
; Installer sections
;=============================================================================

Section "Core Application" SecMain
  SectionIn RO  ; always installed

  SetOutPath "$INSTDIR"

  ; Main executable
  File "${BUILD_BIN_DIR}\${PRODUCT_EXE}"

  ; Runtime DLLs copied next to the exe by CMake POST_BUILD
  ; Use a wildcard so the script works whether GLFW is static or dynamic
  File /nonfatal "${BUILD_BIN_DIR}\*.dll"

  ; License
  File /oname=LICENSE.txt "${SOURCE_DIR}\LICENSE.md"

  ; Write installation metadata to registry
  WriteRegStr   HKLM "${INSTALL_REG}" "InstallDir"    "$INSTDIR"
  WriteRegStr   HKLM "${INSTALL_REG}" "Version"       "${PRODUCT_VERSION}"

  ; Add / Replace Programs entry
  WriteRegStr   HKLM "${UNINSTALL_REG}" "DisplayName"          "${PRODUCT_NAME}"
  WriteRegStr   HKLM "${UNINSTALL_REG}" "DisplayVersion"       "${PRODUCT_VERSION}"
  WriteRegStr   HKLM "${UNINSTALL_REG}" "Publisher"            "${PRODUCT_PUBLISHER}"
  WriteRegStr   HKLM "${UNINSTALL_REG}" "URLInfoAbout"         "${PRODUCT_URL}"
  WriteRegStr   HKLM "${UNINSTALL_REG}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKLM "${UNINSTALL_REG}" "DisplayIcon"          "$INSTDIR\${PRODUCT_EXE},0"
  WriteRegStr   HKLM "${UNINSTALL_REG}" "UninstallString"      '"$INSTDIR\uninstall.exe"'
  WriteRegStr   HKLM "${UNINSTALL_REG}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINSTALL_REG}" "NoModify"             1
  WriteRegDWORD HKLM "${UNINSTALL_REG}" "NoRepair"             1

  ; Uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

SectionEnd

Section "Start Menu Shortcut" SecStartMenu
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortcut  "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" \
                  "$INSTDIR\${PRODUCT_EXE}" "" \
                  "$INSTDIR\${PRODUCT_EXE}" 0
  CreateShortcut  "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall ${PRODUCT_NAME}.lnk" \
                  "$INSTDIR\uninstall.exe"
SectionEnd

Section "Desktop Shortcut" SecDesktop
  CreateShortcut "$DESKTOP\${PRODUCT_NAME}.lnk" \
                 "$INSTDIR\${PRODUCT_EXE}" "" \
                 "$INSTDIR\${PRODUCT_EXE}" 0
SectionEnd

;=============================================================================
; Uninstaller section
;=============================================================================

Section "Uninstall"

  ; Remove shortcuts
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall ${PRODUCT_NAME}.lnk"
  RMDir  "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

  ; Remove installed files
  Delete "$INSTDIR\${PRODUCT_EXE}"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\uninstall.exe"
  RMDir  "$INSTDIR"

  ; Remove registry keys
  DeleteRegKey HKLM "${UNINSTALL_REG}"
  DeleteRegKey HKLM "${INSTALL_REG}"
  DeleteRegKey /ifempty HKLM "Software\${PRODUCT_PUBLISHER}"

SectionEnd

;=============================================================================
; Installer init — enforce 64-bit OS
;=============================================================================

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "This installer requires a 64-bit version of Windows."
    Abort
  ${EndIf}

  ; Upgrade detection: if already installed, ask whether to proceed
  ReadRegStr $R0 HKLM "${INSTALL_REG}" "Version"
  ${If} $R0 != ""
    MessageBox MB_YESNO|MB_ICONQUESTION \
      "${PRODUCT_NAME} $R0 is already installed.$\r$\nDo you want to upgrade to version ${PRODUCT_VERSION}?" \
      IDYES +2
    Abort
  ${EndIf}
FunctionEnd
