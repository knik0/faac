Name "FAAC Windows GUI"
OutFile faac_wingui_install.exe
CRCCheck on
LicenseText "You must read the following license before installing."
LicenseData COPYING
ComponentText "This will install the FAAC Windows GUI on your computer."
InstType Normal
DirText "Please select a location to install the FAAC Windows GUI (or use the default)."
UninstallText "This will uninstall the FAAC Windows GUI from your system."
UninstallExeName uninst.exe
AutoCloseWindow true
SetOverwrite on
SetDateSave on

InstallDir $PROGRAMFILES\FAAC
InstallDirRegKey HKEY_LOCAL_MACHINE SOFTWARE\FAAC ""

Section "FAAC GUI Executable"
SectionIn 1
SetOutPath $INSTDIR
File wingui\Release\faac_wingui.exe

Section "FAAC GUI Shell Extensions"
SectionIn 1
WriteRegStr HKEY_CLASSES_ROOT ".jbl" "" "FAAC.JoblistFile"
WriteRegStr HKEY_CLASSES_ROOT "FAAC.JoblistFile" "" "FAAC JoblistFile"
WriteRegStr HKEY_CLASSES_ROOT "FAAC.JoblistFile\shell" "" "open"
WriteRegStr HKEY_CLASSES_ROOT "FAAC.JoblistFile\DefaultIcon" "" $INSTDIR\faac_wingui.exe,0
WriteRegStr HKEY_CLASSES_ROOT "FAAC.JoblistFile\shell\open\command" "" '$INSTDIR\faac_wingui.exe "%1"'
SectionEnd

Section -post
WriteRegStr HKEY_LOCAL_MACHINE SOFTWARE\FAAC "" $INSTDIR
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\FAAC" "DisplayName" "FAAC Windows GUI"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\FAAC" "UninstallString" '"$INSTDIR\uninst.exe"'
CreateShortCut "$DESKTOP\faac_wingui.lnk" "$INSTDIR\faac_wingui.exe" "" "" 0
SectionEnd

Section Uninstall
DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\FAAC"
DeleteRegKey HKEY_LOCAL_MACHINE SOFTWARE\FAAC
Delete $DESKTOP\faac_wingui.lnk
Delete $INSTDIR\faac_wingui.exe
RMDir $INSTDIR
SectionEnd
