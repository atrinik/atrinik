; NSIS installer script to make Atrinik client win32 installer.
; To make the installer, place this script in your client directory and
; run `makensis client_installer.nsi`

; Installer name.
Name "Atrinik Client 2.0"

; Installer filename.
OutFile "atrinik-client-2.0.exe"

; Installer icon.
Icon "bitmaps\icon.ico"

; Default installation directory.
InstallDir "$PROGRAMFILES\Atrinik Client 2.0"

; Registry key.
InstallDirRegKey HKLM "Software\Atrinik-Client-2.0" "Install_Dir"

; Pages.
Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

; Play nice with UAC in Vista and newer.
RequestExecutionLevel admin

; What to install.
Section "Client (required)"
  SectionIn RO

  SetOutPath $INSTDIR
  File "atrinik.exe"
  File "atrinik.p0"
  File "*.dll"
  File "License"
  File "*.dat"
  File "README.txt"
  File "scripts_autoload"
  FILE "timidity.cfg"

  CreateDirectory $INSTDIR\bitmaps
  SetOutPath $INSTDIR\bitmaps
  File "bitmaps\*.*"

  CreateDirectory $INSTDIR\cache
  SetOutPath $INSTDIR\cache
  File "cache\*.*"

  CreateDirectory $INSTDIR\gfx_user
  SetOutPath $INSTDIR\gfx_user
  File "gfx_user\*.*"

  CreateDirectory $INSTDIR\icons
  SetOutPath $INSTDIR\icons
  File "icons\*.*"

  CreateDirectory $INSTDIR\media
  SetOutPath $INSTDIR\media
  File "media\*.*"

  CreateDirectory $INSTDIR\sfx
  SetOutPath $INSTDIR\sfx
  File "sfx\*.*"

  CreateDirectory $INSTDIR\srv_files
  SetOutPath $INSTDIR\srv_files
  File "srv_files\*.*"

  CreateDirectory $INSTDIR\timidity
  SetOutPath $INSTDIR\timidity
  File /r "timidity\*.*"

  CreateDirectory $INSTDIR\fonts
  SetOutPath $INSTDIR\fonts
  File /r "fonts\*.*"

  SetOutPath $INSTDIR

  WriteRegStr HKLM SOFTWARE\Atrinik-Client-2.0 "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "DisplayName" "Atrinik Client"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "DisplayVersion" "2.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "DisplayIcon" "$INSTDIR\bitmaps\icon.ico"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "Publisher" "Atrinik Team"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "HelpLink" "http://www.atrinik.org"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

; Optional start menu shortcuts.
Section "Start Menu Shortcuts"
  SetShellVarContext all

  CreateDirectory "$SMPROGRAMS\Atrinik Client 2.0"
  CreateShortCut "$SMPROGRAMS\Atrinik Client 2.0\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Atrinik Client 2.0\Atrinik Client.lnk" "$INSTDIR\atrinik.exe" "" "$INSTDIR\bitmaps\icon.ico"
SectionEnd

; Optional desktop shortcut.
Section /o "Desktop Shortcut"
  SetShellVarContext all
  
  CreateShortCut "$DESKTOP\Atrinik Client.lnk" "$INSTDIR\atrinik.exe" "" "$INSTDIR\bitmaps\icon.ico"
SectionEnd

; Uninstaller.
Section "Uninstall"
  SetShellVarContext all

  ; Remove registry keys.
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Atrinik-Client-2.0"
  DeleteRegKey HKLM SOFTWARE\Atrinik-Client-2.0

  Delete /REBOOTOK "$DESKTOP\Atrinik Client.lnk"
  RMDir /r /REBOOTOK "$SMPROGRAMS\Atrinik Client 2.0"
  RMDir /r /REBOOTOK "$INSTDIR"
SectionEnd
