; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CEncoderGeneralPageDialog
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "faac_wingui.h"

ClassCount=10
Class1=CFaac_winguiApp
Class2=CFaac_winguiDlg
Class3=CAboutDlg

ResourceCount=15
Resource1=IDD_ABOUTBOX
Resource2=IDR_MAINFRAME
Resource3=IDD_FAAC_WINGUI_DIALOG
Resource4=IDD_PROPERTIESDUMMYPARENTDIALOG
Resource5=IDD_PROPERTIESTABPARENTDIALOG (English (U.S.))
Class4=CPropertiesTabParentDialog
Resource6=IDD_PROCESSJOBSTATUSDIALOG (English (U.S.))
Class5=CEncoderGeneralPageDialog
Resource7=IDD_FLOATINGPROPERTYDIALOG (English (U.S.))
Class6=CFloatingPropertyDialog
Resource8=IDD_ENCODERGENERALPAGEDIALOG (English (U.S.))
Class7=CPropertiesDummyParentDialog
Resource9=IDD_FAAC_WINGUI_DIALOG (English (U.S.))
Class8=CEncoderId3PageDialog
Resource10=IDD_ENCODERQUALITYPAGEDIALOG
Class9=CEncoderQualityPageDialog
Resource11=IDD_ENCODERID3PAGEDIALOG (English (U.S.))
Class10=CProcessJobStatusDialog
Resource12=IDD_ABOUTBOX (English (U.S.))
Resource13=IDR_TOOLBARMAIN (English (U.S.))
Resource14=IDD_ENCODERQUALITYPAGEDIALOG (German (Germany))
Resource15=IDD_PROPERTIESDUMMYPARENTDIALOG (German (Germany))

[CLS:CFaac_winguiApp]
Type=0
HeaderFile=faac_wingui.h
ImplementationFile=faac_wingui.cpp
Filter=N

[CLS:CFaac_winguiDlg]
Type=0
HeaderFile=faac_winguiDlg.h
ImplementationFile=faac_winguiDlg.cpp
Filter=D
BaseClass=CDialog
VirtualFilter=dWC
LastObject=IDC_BUTTONDUPLICATESELECTED

[CLS:CAboutDlg]
Type=0
HeaderFile=faac_winguiDlg.h
ImplementationFile=faac_winguiDlg.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308352
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889
Class=CAboutDlg


[DLG:IDD_FAAC_WINGUI_DIALOG]
Type=1
ControlCount=3
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC,static,1342308352
Class=CFaac_winguiDlg

[DLG:IDD_FAAC_WINGUI_DIALOG (English (U.S.))]
Type=1
Class=CFaac_winguiDlg
ControlCount=9
Control1=IDOK,button,1342242817
Control2=IDC_LISTJOBS,SysListView32,1350664201
Control3=IDC_BUTTONADDENCODERJOB,button,1342242816
Control4=IDC_BUTTONDELETEJOBS,button,1342242816
Control5=IDC_BUTTONPROCESSSELECTED,button,1342242816
Control6=IDC_CHECKREMOVEPROCESSEDJOBS,button,1342242819
Control7=IDC_BUTTONSAVEJOBLIST,button,1342242816
Control8=IDC_BUTTONLOADJOBLIST,button,1342242816
Control9=IDC_BUTTONDUPLICATESELECTED,button,1342242816

[DLG:IDD_ABOUTBOX (English (U.S.))]
Type=1
Class=CAboutDlg
ControlCount=5
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889
Control5=IDC_STATIC,static,1342308352

[CLS:CPropertiesTabParentDialog]
Type=0
HeaderFile=PropertiesTabParentDialog.h
ImplementationFile=PropertiesTabParentDialog.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CPropertiesTabParentDialog

[DLG:IDD_PROPERTIESTABPARENTDIALOG (English (U.S.))]
Type=1
Class=CPropertiesTabParentDialog
ControlCount=3
Control1=IDC_TAB1,SysTabControl32,1342177280
Control2=IDC_LABELNOSELECTION,static,1342308352
Control3=IDC_LABELDEBUGTAG,static,1073872896

[DLG:IDD_ENCODERGENERALPAGEDIALOG (English (U.S.))]
Type=1
Class=CEncoderGeneralPageDialog
ControlCount=14
Control1=IDC_STATIC,static,1342308352
Control2=IDC_EDITSOURCEDIR,edit,1350631552
Control3=IDC_BUTTONBROWSESOURCEDIR,button,1342242816
Control4=IDC_STATIC,static,1342308352
Control5=IDC_EDITSOURCEFILE,edit,1350631552
Control6=IDC_BUTTONBROWSESOURCEFILE,button,1342242816
Control7=IDC_CHECKRECURSIVE,button,1342242819
Control8=IDC_STATIC,static,1342308352
Control9=IDC_EDITTARGETDIR,edit,1350631552
Control10=IDC_BUTTONBROWSETARGETDIR,button,1342242816
Control11=IDC_STATIC,static,1342308352
Control12=IDC_EDITTARGETFILE,edit,1350631552
Control13=IDC_BUTTONBROWSETARGETFILE,button,1342242816
Control14=IDC_BUTTON1,button,1073807361

[CLS:CEncoderGeneralPageDialog]
Type=0
HeaderFile=EncoderGeneralPageDialog.h
ImplementationFile=EncoderGeneralPageDialog.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CEncoderGeneralPageDialog

[DLG:IDD_FLOATINGPROPERTYDIALOG (English (U.S.))]
Type=1
Class=CFloatingPropertyDialog
ControlCount=1
Control1=IDC_LABELDEBUGTAG,static,1073872896

[CLS:CFloatingPropertyDialog]
Type=0
HeaderFile=FloatingPropertyDialog.h
ImplementationFile=FloatingPropertyDialog.cpp
BaseClass=CDialog
Filter=D
LastObject=CFloatingPropertyDialog
VirtualFilter=dWC

[DLG:IDD_PROPERTIESDUMMYPARENTDIALOG]
Type=1
Class=CPropertiesDummyParentDialog
ControlCount=3
Control1=IDC_STATIC,static,1073872896
Control2=IDC_STATIC,static,1073872896
Control3=IDC_LABELDEBUGTAG,static,1073872896

[CLS:CPropertiesDummyParentDialog]
Type=0
HeaderFile=PropertiesDummyParentDialog.h
ImplementationFile=PropertiesDummyParentDialog.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC

[DLG:IDD_ENCODERID3PAGEDIALOG (English (U.S.))]
Type=1
Class=CEncoderId3PageDialog
ControlCount=24
Control1=IDC_STATIC,static,1342308352
Control2=IDC_STATIC,static,1342308352
Control3=IDC_STATIC,static,1342308352
Control4=IDC_STATIC,static,1342308352
Control5=IDC_STATIC,static,1342308352
Control6=IDC_EDITARTIST,edit,1350631552
Control7=IDC_EDITTRACK,edit,1350631552
Control8=IDC_EDITTITLE,edit,1350631552
Control9=IDC_EDITURL,edit,1350631552
Control10=IDC_STATIC,static,1342308352
Control11=IDC_EDITALBUM,edit,1350631552
Control12=IDC_STATIC,static,1342308352
Control13=IDC_EDITYEAR,edit,1350631552
Control14=IDC_STATIC,static,1342308352
Control15=IDC_EDITCOMPOSER,edit,1350631552
Control16=IDC_STATIC,static,1342308352
Control17=IDC_EDITORIGINALARTIST,edit,1350631552
Control18=IDC_CHECKCOPYRIGHT,button,1342242819
Control19=IDC_STATIC,static,1342308352
Control20=IDC_EDITCOMMENT,edit,1352728580
Control21=IDC_STATIC,static,1342308352
Control22=IDC_EDITENCODEDBY,edit,1350631552
Control23=IDC_COMBOGENRE,combobox,1344340227
Control24=IDC_BUTTON1,button,1073807361

[CLS:CEncoderId3PageDialog]
Type=0
HeaderFile=EncoderId3PageDialog.h
ImplementationFile=EncoderId3PageDialog.cpp
BaseClass=CDialog
Filter=D
LastObject=CEncoderId3PageDialog
VirtualFilter=dWC

[DLG:IDD_ENCODERQUALITYPAGEDIALOG]
Type=1
Class=CEncoderQualityPageDialog
ControlCount=15
Control1=IDC_STATIC,static,1342373888
Control2=IDC_EDITBITRATE,edit,1350631552
Control3=IDC_STATIC,static,1342308352
Control4=IDC_STATIC,static,1342308352
Control5=IDC_EDITBANDWIDTH,edit,1350631552
Control6=IDC_STATIC,static,1342308352
Control7=IDC_CHECKMIDSIDE,button,1342242819
Control8=IDC_CHECKUSETNS,button,1342242819
Control9=IDC_CHECKUSELTP,button,1342242819
Control10=IDC_CHECKUSELFE,button,1342242819
Control11=IDC_STATIC,button,1342177287
Control12=IDC_RADIOAACPROFILELC,button,1342373897
Control13=IDC_RADIOAACPROFILEMAIN,button,1342177289
Control14=IDC_RADIOAACPROFILESSR,button,1342177289
Control15=IDC_BUTTON1,button,1073807361

[CLS:CEncoderQualityPageDialog]
Type=0
HeaderFile=EncoderQualityPageDialog.h
ImplementationFile=EncoderQualityPageDialog.cpp
BaseClass=CDialog
Filter=D
LastObject=CEncoderQualityPageDialog
VirtualFilter=dWC

[DLG:IDD_PROCESSJOBSTATUSDIALOG (English (U.S.))]
Type=1
Class=CProcessJobStatusDialog
ControlCount=6
Control1=IDC_LABELTOPSTATUSTEXT,static,1342308353
Control2=IDC_LABELBOTTOMSTATUSTEXT,static,1342308353
Control3=IDC_PROGRESS1,msctls_progress32,1350565888
Control4=IDC_BUTTONABORT,button,1073807360
Control5=IDC_BUTTONPAUSE,button,1073807360
Control6=IDC_BUTTONCONTINUE,button,1208025088

[CLS:CProcessJobStatusDialog]
Type=0
HeaderFile=ProcessJobStatusDialog.h
ImplementationFile=ProcessJobStatusDialog.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CProcessJobStatusDialog

[TB:IDR_TOOLBARMAIN (English (U.S.))]
Type=1
Class=?
Command1=ID_BUTTON32771
Command2=ID_BUTTON32772
Command3=ID_BUTTON32773
Command4=ID_BUTTON32774
CommandCount=4

[DLG:IDD_PROPERTIESDUMMYPARENTDIALOG (German (Germany))]
Type=1
Class=?
ControlCount=3
Control1=IDC_STATIC,static,1073872896
Control2=IDC_STATIC,static,1073872896
Control3=IDC_LABELDEBUGTAG,static,1073872896

[DLG:IDD_ENCODERQUALITYPAGEDIALOG (German (Germany))]
Type=1
Class=?
ControlCount=15
Control1=IDC_STATIC,static,1342373888
Control2=IDC_EDITBITRATE,edit,1350631552
Control3=IDC_STATIC,static,1342308352
Control4=IDC_STATIC,static,1342308352
Control5=IDC_EDITBANDWIDTH,edit,1350631552
Control6=IDC_STATIC,static,1342308352
Control7=IDC_CHECKMIDSIDE,button,1342242819
Control8=IDC_CHECKUSETNS,button,1342242819
Control9=IDC_CHECKUSELTP,button,1342242819
Control10=IDC_CHECKUSELFE,button,1342242819
Control11=IDC_STATIC,button,1342177287
Control12=IDC_RADIOAACPROFILELC,button,1342373897
Control13=IDC_RADIOAACPROFILEMAIN,button,1342177289
Control14=IDC_RADIOAACPROFILESSR,button,1342177289
Control15=IDC_BUTTON1,button,1073807361

