!include "MUI2.nsh"

; ======================
; BASIC
; ======================
Name "Context Menu Swapper V3"
OutFile "Context_Menu_Swapper_V3_Setup.exe"
InstallDir "$PROGRAMFILES\ContextMenuSwapperV3"
RequestExecutionLevel admin
Icon "ico.ico"

; ======================
; MODERN UI
; ======================
!define MUI_ABORTWARNING
!define MUI_ICON "ico.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"

; ======================
; INSTALL SECTION
; ======================
Section "Install"

SetOutPath "$INSTDIR"

; --- MAIN APP ---
File "Context_Menu_Swapper_V3.exe"
File "ico.ico"

; --- QT ---
File "Qt6Core.dll"
File "Qt6Gui.dll"
File "Qt6Widgets.dll"

; --- QT PLUGIN ---
SetOutPath "$INSTDIR\platforms"
File "platforms\qwindows.dll"

; --- RUNTIME DLLS (FLAT STRUCTURE) ---
SetOutPath "$INSTDIR"

File "libb2-1.dll"
File "libbrotlicommon.dll"
File "libbrotlidec.dll"
File "libbz2-1.dll"
File "libdouble-conversion.dll"
File "libfreetype-6.dll"
File "libgcc_s_seh-1.dll"
File "libglib-2.0-0.dll"
File "libgmp-10.dll"
File "libgraphite2.dll"
File "libharfbuzz-0.dll"
File "libiconv-2.dll"
File "libicudt78.dll"
File "libicuin78.dll"
File "libicuuc78.dll"
File "libintl-8.dll"
File "libisl-23.dll"
File "libmd4c.dll"
File "libmpc-3.dll"
File "libmpfr-6.dll"
File "libpcre2-8-0.dll"
File "libpcre2-16-0.dll"
File "libpng16-16.dll"
File "libstdc++-6.dll"
File "libwinpthread-1.dll"
File "libzstd.dll"
File "zlib1.dll"

; ======================
; UNINSTALL GENERATION (FIX 6020)
; ======================
WriteUninstaller "$INSTDIR\Uninstall.exe"

; ======================
; SHORTCUTS
; ======================
CreateDirectory "$SMPROGRAMS\Context Menu Swapper V3"

CreateShortcut "$SMPROGRAMS\Context Menu Swapper V3\Context Menu Swapper V3.lnk" "$INSTDIR\Context_Menu_Swapper_V3.exe" "$INSTDIR\ico.ico"

CreateShortcut "$DESKTOP\Context Menu Swapper V3.lnk" "$INSTDIR\Context_Menu_Swapper_V3.exe" "$INSTDIR\ico.ico"

SectionEnd

; ======================
; UNINSTALL SECTION
; ======================
Section "Uninstall"

Delete "$INSTDIR\Context_Menu_Swapper_V3.exe"
Delete "$INSTDIR\ico.ico"

Delete "$INSTDIR\Qt6Core.dll"
Delete "$INSTDIR\Qt6Gui.dll"
Delete "$INSTDIR\Qt6Widgets.dll"

Delete "$INSTDIR\Uninstall.exe"

Delete "$INSTDIR\*.dll"

RMDir /r "$INSTDIR"

Delete "$DESKTOP\Context Menu Swapper V3.lnk"

RMDir /r "$SMPROGRAMS\Context Menu Swapper V3"

SectionEnd
