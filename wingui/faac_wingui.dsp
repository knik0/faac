# Microsoft Developer Studio Project File - Name="faac_wingui" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=faac_wingui - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "faac_wingui.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "faac_wingui.mak" CFG="faac_wingui - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "faac_wingui - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "faac_wingui - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "faac_wingui - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../include" /I "C:\Eigene Dateien\Programmierung\C_Projekte\Downloaded_GNU\libsndfile\libsndfile-0.0.22\src" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "faac_wingui - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../include" /I "C:\Eigene Dateien\Programmierung\C_Projekte\Downloaded_GNU\libsndfile\libsndfile-0.0.22\src" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "faac_wingui - Win32 Release"
# Name "faac_wingui - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\AbstractJob.cpp
# End Source File
# Begin Source File

SOURCE=.\AbstractPageCtrlContent.cpp
# End Source File
# Begin Source File

SOURCE=.\AbstractPropertyPageContents.cpp
# End Source File
# Begin Source File

SOURCE=.\ConcreteJobBase.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderGeneralPageDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderGeneralPropertyPageContents.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderId3PageDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderId3PropertyPageContents.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderJob.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderJobProcessingManager.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderQualityPageDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EncoderQualityPropertyPageContents.cpp
# End Source File
# Begin Source File

SOURCE=.\faac_wingui.cpp
# End Source File
# Begin Source File

SOURCE=.\faac_wingui.rc
# End Source File
# Begin Source File

SOURCE=.\faac_winguiDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\FaacWinguiProgramSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\FileListQueryManager.cpp
# End Source File
# Begin Source File

SOURCE=.\FileMaskAssembler.cpp
# End Source File
# Begin Source File

SOURCE=.\FilePathCalc.cpp
# End Source File
# Begin Source File

SOURCE=.\FileSerializable.cpp
# End Source File
# Begin Source File

SOURCE=.\FileSerializableJobList.cpp
# End Source File
# Begin Source File

SOURCE=.\FloatingPropertyDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\FolderDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Id3TagInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\Job.cpp
# End Source File
# Begin Source File

SOURCE=.\JobList.cpp
# End Source File
# Begin Source File

SOURCE=.\JobListCtrlDescribable.cpp
# End Source File
# Begin Source File

SOURCE=.\JobListsToConfigureSaver.cpp
# End Source File
# Begin Source File

SOURCE=.\JobListUpdatable.cpp
# End Source File
# Begin Source File

SOURCE=.\ListCtrlStateSaver.cpp
# End Source File
# Begin Source File

SOURCE=.\Listobj.cpp
# End Source File
# Begin Source File

SOURCE=.\PageCheckboxCtrlContent.cpp
# End Source File
# Begin Source File

SOURCE=.\PageComboBoxCtrlContent.cpp
# End Source File
# Begin Source File

SOURCE=.\PageEditCtrlContent.cpp
# End Source File
# Begin Source File

SOURCE=.\PageRadioGroupCtrlContent.cpp
# End Source File
# Begin Source File

SOURCE=.\ProcessingStartStopPauseInteractable.cpp
# End Source File
# Begin Source File

SOURCE=.\ProcessingStatusDialogInfoFeedbackCallbackInterface.cpp
# End Source File
# Begin Source File

SOURCE=.\ProcessJobStatusDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\PropertiesDummyParentDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\PropertiesTabParentDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\RecursiveDirectoryTraverser.cpp
# End Source File
# Begin Source File

SOURCE=.\SourceTargetFilePair.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\SupportedPropertyPagesData.cpp
# End Source File
# Begin Source File

SOURCE=.\TItemList.cpp
# End Source File
# Begin Source File

SOURCE=.\WindowUtil.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\AbstractJob.h
# End Source File
# Begin Source File

SOURCE=.\AbstractPageCtrlContent.h
# End Source File
# Begin Source File

SOURCE=.\AbstractPropertyPageContents.h
# End Source File
# Begin Source File

SOURCE=.\ConcreteJobBase.h
# End Source File
# Begin Source File

SOURCE=.\EncoderGeneralPageDialog.h
# End Source File
# Begin Source File

SOURCE=.\EncoderGeneralPropertyPageContents.h
# End Source File
# Begin Source File

SOURCE=.\EncoderId3PageDialog.h
# End Source File
# Begin Source File

SOURCE=.\EncoderId3PropertyPageContents.h
# End Source File
# Begin Source File

SOURCE=.\EncoderJob.h
# End Source File
# Begin Source File

SOURCE=.\EncoderJobProcessingManager.h
# End Source File
# Begin Source File

SOURCE=.\EncoderQualityPageDialog.h
# End Source File
# Begin Source File

SOURCE=.\EncoderQualityPropertyPageContents.h
# End Source File
# Begin Source File

SOURCE=..\include\faac.h
# End Source File
# Begin Source File

SOURCE=.\faac_wingui.h
# End Source File
# Begin Source File

SOURCE=.\faac_winguiDlg.h
# End Source File
# Begin Source File

SOURCE=.\FaacWinguiProgramSettings.h
# End Source File
# Begin Source File

SOURCE=.\FileListQueryManager.h
# End Source File
# Begin Source File

SOURCE=.\FileMaskAssembler.h
# End Source File
# Begin Source File

SOURCE=.\FilePathCalc.h
# End Source File
# Begin Source File

SOURCE=.\FileSerializable.h
# End Source File
# Begin Source File

SOURCE=.\FileSerializableJobList.h
# End Source File
# Begin Source File

SOURCE=.\FloatingPropertyDialog.h
# End Source File
# Begin Source File

SOURCE=.\FolderDialog.h
# End Source File
# Begin Source File

SOURCE=.\Id3TagInfo.h
# End Source File
# Begin Source File

SOURCE=.\Job.h
# End Source File
# Begin Source File

SOURCE=.\JobList.h
# End Source File
# Begin Source File

SOURCE=.\JobListCtrlDescribable.h
# End Source File
# Begin Source File

SOURCE=.\JobListsToConfigureSaver.h
# End Source File
# Begin Source File

SOURCE=.\JobListUpdatable.h
# End Source File
# Begin Source File

SOURCE=.\ListCtrlStateSaver.h
# End Source File
# Begin Source File

SOURCE=.\listobj.h
# End Source File
# Begin Source File

SOURCE=.\PageCheckboxCtrlContent.h
# End Source File
# Begin Source File

SOURCE=.\PageComboBoxCtrlContent.h
# End Source File
# Begin Source File

SOURCE=.\PageEditCtrlContent.h
# End Source File
# Begin Source File

SOURCE=.\PageRadioGroupCtrlContent.h
# End Source File
# Begin Source File

SOURCE=.\ProcessingStartStopPauseInteractable.h
# End Source File
# Begin Source File

SOURCE=.\ProcessingStatusDialogInfoFeedbackCallbackInterface.h
# End Source File
# Begin Source File

SOURCE=.\ProcessJobStatusDialog.h
# End Source File
# Begin Source File

SOURCE=.\PropertiesDummyParentDialog.h
# End Source File
# Begin Source File

SOURCE=.\PropertiesTabParentDialog.h
# End Source File
# Begin Source File

SOURCE=.\RecursiveDirectoryTraverser.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE="E:\Program Files\Microsoft Visual Studio\VC98\Include\sndfile.h"
# End Source File
# Begin Source File

SOURCE=.\SourceTargetFilePair.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\SupportedPropertyPagesData.h
# End Source File
# Begin Source File

SOURCE=.\TItemList.h
# End Source File
# Begin Source File

SOURCE=.\WindowUtil.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\faac_wingui.ico
# End Source File
# Begin Source File

SOURCE=.\res\faac_wingui.rc2
# End Source File
# Begin Source File

SOURCE=.\res\toolbarm.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
