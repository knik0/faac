// faac_winguiDlg.cpp : implementation file
// Author: Torsten Landmann
//
///////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "faac_winguiDlg.h"
#include "WindowUtil.h"
#include "FloatingPropertyDialog.h"
#include "FileListQueryManager.h"
#include "FileMaskAssembler.h"
#include "FileSerializableJobList.h"
#include "FolderDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//#define EXTENDED_JOB_TRACKING_DURING_PROCESSING		// doesn't work, yet

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFaac_winguiDlg dialog

CFaac_winguiDlg::CFaac_winguiDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CFaac_winguiDlg::IDD, pParent),
	m_iListCtrlUpdatesCounter(-1),
	m_poCurrentStandardFile(0),
	m_poHiddenPropertiesWindow(0)
{
	//{{AFX_DATA_INIT(CFaac_winguiDlg)
	m_bCheckRemoveProcessedJobs = FALSE;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CFaac_winguiDlg::~CFaac_winguiDlg()
{
	ReleaseStandardFile();
}

void CFaac_winguiDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFaac_winguiDlg)
	DDX_Control(pDX, IDC_BUTTONEXPANDFILTERJOB, m_ctrlButtonExpandFilterJob);
	DDX_Control(pDX, IDC_BUTTONOPENPROPERTIES, m_ctrlButtonOpenProperties);
	DDX_Control(pDX, IDC_LISTJOBS, m_ctrlListJobs);
	DDX_Check(pDX, IDC_CHECKREMOVEPROCESSEDJOBS, m_bCheckRemoveProcessedJobs);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CFaac_winguiDlg, CDialog)
	//{{AFX_MSG_MAP(CFaac_winguiDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTONADDENCODERJOB, OnButtonAddEncoderJob)
	ON_NOTIFY(NM_CLICK, IDC_LISTJOBS, OnClickListJobs)
	ON_NOTIFY(LVN_KEYDOWN, IDC_LISTJOBS, OnKeydownListJobs)
	ON_BN_CLICKED(IDC_BUTTONDELETEJOBS, OnButtonDeleteJobs)
	ON_BN_CLICKED(IDC_BUTTONPROCESSSELECTED, OnButtonProcessSelected)
	ON_NOTIFY(NM_RCLICK, IDC_LISTJOBS, OnRclickListJobs)
	ON_BN_CLICKED(IDC_BUTTONSAVEJOBLIST, OnButtonSaveJobList)
	ON_BN_CLICKED(IDC_BUTTONLOADJOBLIST, OnButtonLoadJobList)
	ON_BN_CLICKED(IDC_BUTTONDUPLICATESELECTED, OnButtonDuplicateSelected)
	ON_BN_CLICKED(IDC_BUTTONPROCESSALL, OnButtonProcessAll)
	ON_BN_CLICKED(IDC_BUTTONOPENPROPERTIES, OnButtonOpenProperties)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_BUTTONEXPANDFILTERJOB, OnButtonExpandFilterJob)
	ON_BN_CLICKED(IDC_BUTTONADDFILTERENCODERJOB, OnButtonAddFilterEncoderJob)
	ON_BN_CLICKED(IDC_BUTTONADDEMPTYENCODERJOB, OnButtonAddEmptyEncoderJob)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFaac_winguiDlg message handlers

BOOL CFaac_winguiDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	{
		CString oTypeColumn;
		oTypeColumn.LoadString(IDS_JobTypeColumn);
		CString oInfoColumn;
		oInfoColumn.LoadString(IDS_JobInfoColumn);
		CString oProcessInfoColumn;
		oProcessInfoColumn.LoadString(IDS_JobProcessInfoColumn);
		CWindowUtil::AddListCtrlColumn(&m_ctrlListJobs, 0, oTypeColumn, 0.075);
		CWindowUtil::AddListCtrlColumn(&m_ctrlListJobs, 1, oInfoColumn, 0.75);
		CWindowUtil::AddListCtrlColumn(&m_ctrlListJobs, 2, oProcessInfoColumn, 0.9999);
		CWindowUtil::SetListCtrlFullRowSelectStyle(&m_ctrlListJobs);
		ReFillInJobListCtrl();

		// create a floating property window
		CFloatingPropertyDialog::CreateFloatingPropertiesDummyParentDialog();
		m_ctrlButtonOpenProperties.ShowWindow(SW_HIDE);		// property window is open

		// make sure the floating window is initialized properly
		OnJobListCtrlSelChange();


		// load file specified on the command line (if any)
		{
			// Parse command line for standard shell commands, DDE, file open
			CCommandLineInfo cmdInfo;
			AfxGetApp()->ParseCommandLine(cmdInfo);
			if (cmdInfo.m_nShellCommand==CCommandLineInfo::FileNew)	// prevent opening a new file on program start
			{
				cmdInfo.m_nShellCommand=CCommandLineInfo::FileNothing;
			}
			else if (cmdInfo.m_nShellCommand==CCommandLineInfo::FileOpen)
			{
				// must load a job list
				LoadJobList(cmdInfo.m_strFileName);
			}
		}
	}
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CFaac_winguiDlg::HidePropertiesWindow(CWnd *poPropertiesWindow)
{
	// must not hide two property windows at a time
	ASSERT(m_poHiddenPropertiesWindow==0);

	m_poHiddenPropertiesWindow=poPropertiesWindow;
	m_poHiddenPropertiesWindow->ShowWindow(SW_HIDE);
	m_ctrlButtonOpenProperties.ShowWindow(SW_SHOW);
}

void CFaac_winguiDlg::ShowPropertiesWindow()
{
	m_poHiddenPropertiesWindow->ShowWindow(SW_SHOW);
	m_poHiddenPropertiesWindow=0;
	m_ctrlButtonOpenProperties.ShowWindow(SW_HIDE);
}

void CFaac_winguiDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CFaac_winguiDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CFaac_winguiDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CFaac_winguiDlg::ReFillInJobListCtrl(CListCtrlStateSaver *poSelectionStateToUse, bool bSimpleUpdate)
{
	if (m_iListCtrlUpdatesCounter>=0)
	{
		m_iListCtrlUpdatesCounter++;
		return;
	}

	CJobList *poJobList=GetGlobalJobList();

	if (!bSimpleUpdate)
	{
		// complete update

		// save the current list selection
		bool bMustDeleteSelection=false;
		if (poSelectionStateToUse==0)
		{
			poSelectionStateToUse=new CListCtrlStateSaver(&m_ctrlListJobs);
			bMustDeleteSelection=true;
		}

		CWindowUtil::DeleteAllListCtrlItems(&m_ctrlListJobs);

		// loop through all jobs and add them to the list control
		CBListReader oReader(poJobList->BeginReadEx());
		CJob *poCurrentJob;
		long lCurrentJobIndex;
		while ((poCurrentJob=poJobList->GetNextJob(oReader, &lCurrentJobIndex))!=0)
		{
			long lCurListItem=CWindowUtil::AddListCtrlItem(&m_ctrlListJobs, poCurrentJob->DescribeJobTypeShort(), lCurrentJobIndex);
			CWindowUtil::SetListCtrlItem(&m_ctrlListJobs, lCurListItem, 1, poCurrentJob->DescribeJob());
			CWindowUtil::SetListCtrlItem(&m_ctrlListJobs, lCurListItem, 2, poCurrentJob->GetProcessingOutcomeString());
		}

		// restore the previous list selection
		poSelectionStateToUse->RestoreState(&m_ctrlListJobs);
		if (bMustDeleteSelection)
		{
			delete poSelectionStateToUse;
		}
	}
	else
	{
		// simple update
		int iNumberOfListCtrlItems=m_ctrlListJobs.GetItemCount();

		for (int iCurItem=0; iCurItem<iNumberOfListCtrlItems; iCurItem++)
		{
			long lItemLParam=m_ctrlListJobs.GetItemData(iCurItem);

			// get the job that belongs to the item
			CJob *poCurJob=poJobList->GetJob(lItemLParam);
			CWindowUtil::SetListCtrlItem(&m_ctrlListJobs, iCurItem, 1, poCurJob->DescribeJob());
			CWindowUtil::SetListCtrlItem(&m_ctrlListJobs, iCurItem, 2, poCurJob->GetProcessingOutcomeString());
		}
	}
}

void CFaac_winguiDlg::EnableExpandFilterJobButton(bool bEnable)
{
	// before enabling it make sure only one job is selected
	if (bEnable && CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, true).GetNumber()!=1)
	{
		// ignore or better to say disable
		m_ctrlButtonExpandFilterJob.EnableWindow(FALSE);
		return;
	}
	m_ctrlButtonExpandFilterJob.EnableWindow(bEnable ? TRUE : FALSE);
}

void CFaac_winguiDlg::OnJobListCtrlUserAction()
{
	// check if the selection has changed
	TItemList<long> m_oSelectedJobsIds=CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, true);

	if (((m_lLastJobListSelection-m_oSelectedJobsIds).GetNumber()==0) &&
		((m_oSelectedJobsIds-m_lLastJobListSelection).GetNumber()==0))
	{
		// the selection hasn't changed
		return;
	}

	// the selection has changed
	m_iListCtrlUpdatesCounter=0;		// make sure no more than one list update is done
	m_lLastJobListSelection=m_oSelectedJobsIds;
	OnJobListCtrlSelChange();
	m_ctrlListJobs.SetFocus();

	if (m_iListCtrlUpdatesCounter>0)
	{
		// need a refill
		m_iListCtrlUpdatesCounter=-1;
		ReFillInJobListCtrl();
	}
	m_iListCtrlUpdatesCounter=-1;
}

void CFaac_winguiDlg::OnButtonAddEncoderJob() 
{
	CJobList *poJobList=GetGlobalJobList();

	CString oSourceFileExtension;
	CString oTargetFileExtension;
	oSourceFileExtension.LoadString(IDS_EndSourceFileStandardExtension);
	oTargetFileExtension.LoadString(IDS_EndTargetFileStandardExtension);

	// assemble the filter string
	CString oFilter;
	{
		// assemble a list of allowed filters
		TItemList<int> oFilters;
		oFilters.AddNewElem(IDS_WavFilesFilter, 1);
		oFilters.AddNewElem(IDS_AllFilesFilter, 2);
		oFilter=CFileMaskAssembler::GetFileMask(oFilters);
	}

	TItemList<CString> oUserSelection=
		CFileListQueryManager::GetFilePaths(
		true,
		&GetGlobalProgramSettings()->m_oDefaultEncSourceDir,
		oSourceFileExtension,
		oFilter);

	CBListReader oReader(oUserSelection);
	CString oCurString;
	TItemList<long> m_oIndicesOfAddedItems;
	while (oUserSelection.GetNextElemContent(oReader, oCurString))
	{
		// create a new job for the current file
		CEncoderJob oNewJob;
		GetGlobalProgramSettings()->ApplyToJob(oNewJob);

		CString oSourceDir(oCurString);
		CFilePathCalc::MakePath(oSourceDir);
		CString oSourceFileName;
		CFilePathCalc::ExtractFileName(oCurString, oSourceFileName);
		CString oTargetDir(oNewJob.GetFiles().GetTargetFileDirectory());
		CString oDummy;
		CString oTargetFileName;
		CFilePathCalc::SplitFileAndExtension(oSourceFileName, oTargetFileName, oDummy);
		oTargetFileName+=_T(".")+oTargetFileExtension;

		oNewJob.GetFiles().SetSourceFileDirectory(oSourceDir);
		oNewJob.GetFiles().SetSourceFileName(oSourceFileName);
		oNewJob.GetFiles().SetTargetFileDirectory(oTargetDir);
		oNewJob.GetFiles().SetTargetFileName(oTargetFileName);

		long lNewIndex=poJobList->AddJob(oNewJob);
		m_oIndicesOfAddedItems.AddNewElem(lNewIndex);
	}

	
	CListCtrlStateSaver oSelection;
	oSelection.SetToSelected(m_oIndicesOfAddedItems);
	ReFillInJobListCtrl(&oSelection, false);

	m_ctrlListJobs.SetFocus();

	OnJobListCtrlUserAction();
}

void CFaac_winguiDlg::OnJobListCtrlSelChange(TItemList<long> *poNewSelection)
{
	// determine which jobs are selected
	TItemList<long> oSelectedJobsIds;
	if (poNewSelection!=0)
	{
		oSelectedJobsIds=*poNewSelection;
	}
	else
	{
		oSelectedJobsIds=CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, true);
	}
	CJobList &oJobs=((CFaac_winguiApp*)AfxGetApp())->GetGlobalJobList();

	CPropertiesDummyParentDialog *poPropertiesDialog=((CFaac_winguiApp*)AfxGetApp())->GetGlobalPropertiesDummyParentDialogSingleton();
	if (poPropertiesDialog==0)
	{
		// nothing to do here
		return;
	}

	// ask them all which pages they support
	CBListReader oReader(oSelectedJobsIds);
	long lCurJobIndex;
	CSupportedPropertyPagesData* poSupportedPages=0;
	TItemList<CJob*> oSelectedJobs;
	while (oSelectedJobsIds.GetNextElemContent(oReader, lCurJobIndex))
	{
		CJob *poCurJob=oJobs.GetJob(lCurJobIndex);
		if (poSupportedPages==0)
		{
			poSupportedPages=new CSupportedPropertyPagesData;
			*poSupportedPages=poCurJob->GetSupportedPropertyPages();
		}
		else
		{
			(*poSupportedPages)*=poCurJob->GetSupportedPropertyPages();
		}

		oSelectedJobs.AddNewElem(poCurJob, lCurJobIndex);
	}

	if (poSupportedPages==0)
	{
		// no list item selected -> no page accepted
		poSupportedPages=new CSupportedPropertyPagesData;
	}

	// display the property pages
	poPropertiesDialog->SetContent(*poSupportedPages, oSelectedJobs, this);

	if (poSupportedPages!=0)
	{
		delete poSupportedPages;
	}

	// enable/disable the "expand filter job" button
	{
		bool bButtonEnabled=true;
		if (oSelectedJobsIds.GetNumber()!=1)
		{
			bButtonEnabled=false;
		}
		else
		{
			long lSelectedJobId;
			long lDummy;
			oSelectedJobsIds.GetFirstElemContent(lSelectedJobId, lDummy);
			CJob *poJob=oJobs.GetJob(lSelectedJobId);
			if (poJob->GetEncoderJob()==0)
			{
				bButtonEnabled=false;
			}
			else if (!CFilePathCalc::IsValidFileMask(poJob->GetEncoderJob()->GetFiles().GetSourceFileName()))
			{
				bButtonEnabled=false;
			}
		}

		m_ctrlButtonExpandFilterJob.EnableWindow(bButtonEnabled ? TRUE : FALSE);
	}
}

void CFaac_winguiDlg::OnClickListJobs(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	OnJobListCtrlUserAction();
	
	*pResult = 0;
}

void CFaac_winguiDlg::OnKeydownListJobs(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDow = (LV_KEYDOWN*)pNMHDR;
	// TODO: Add your control notification handler code here

	OnJobListCtrlUserAction();
	
	*pResult = 0;
}

void CFaac_winguiDlg::OnButtonDeleteJobs() 
{
	// TODO: Add your control notification handler code here

	// make sure the property pages won't crash
	OnJobListCtrlSelChange(&TItemList<long>());

	// delete the selection
	TItemList<long> oCurSelection(CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, false));
	GetGlobalJobList()->DeleteElems(oCurSelection);

	// update the list control
	ReFillInJobListCtrl(0, false);
}

void CFaac_winguiDlg::OnButtonProcessSelected() 
{
	if (!UpdateData(TRUE)) return;

	TItemList<long> oCurSelection(CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, false));
	ProcessJobs(oCurSelection, m_bCheckRemoveProcessedJobs!=0);

	CJobList *poJobList=GetGlobalJobList();
}

void CFaac_winguiDlg::OnRclickListJobs(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	OnJobListCtrlUserAction();
	
	*pResult = 0;
}

void CFaac_winguiDlg::OnButtonSaveJobList() 
{
	// TODO: Add your control notification handler code here

	//UpdateData(TRUE);
	// determine the standard file
	CString *poStandardFile=m_poCurrentStandardFile;

	// assemble the filter string
	CString oFilter;
	{
		// assemble a list of allowed filters
		TItemList<int> oFilters;
		oFilters.AddNewElem(IDS_JobListFileFilter, 1);
		oFilters.AddNewElem(IDS_AllFilesFilter, 2);
		oFilter=CFileMaskAssembler::GetFileMask(oFilters);
	}

	// find out the default extension
	CString oDefaultExt;
	oDefaultExt.LoadString(IDS_JobListFileFilter);

	CFileDialog oOpenDialog(
		FALSE,										// file save mode
		oDefaultExt,								// default extension
		(poStandardFile!=0 ? *poStandardFile : (LPCTSTR)0),	// default file
		OFN_HIDEREADONLY,
		oFilter);
	if (oOpenDialog.DoModal()==IDOK)
	{
		// the file has been selected
		ReleaseStandardFile();
		m_poCurrentStandardFile=new CString(oOpenDialog.GetPathName());
		SaveJobList(*m_poCurrentStandardFile);
	}
	else
	{
		// user abort or error
		if (CommDlgExtendedError()!=0)
		{
			// an error occured
			AfxMessageBox(IDS_ErrorDuringFileSelection);
		}
	}

	//UpdateData(FALSE);
}

void CFaac_winguiDlg::OnButtonLoadJobList() 
{
	// TODO: Add your control notification handler code here
	//UpdateData(TRUE);
	// determine the standard file
	CString *poStandardFile=m_poCurrentStandardFile;

	// assemble the filter string
	CString oFilter;
	{
		// assemble a list of allowed filters
		TItemList<int> oFilters;
		oFilters.AddNewElem(IDS_JobListFileFilter, 1);
		oFilters.AddNewElem(IDS_AllFilesFilter, 2);
		oFilter=CFileMaskAssembler::GetFileMask(oFilters);
	}

	// find out the default extension
	CString oDefaultExt;
	oDefaultExt.LoadString(IDS_JobListFileFilter);

	CFileDialog oOpenDialog(
		TRUE,										// file open mode
		oDefaultExt,								// default extension
		(poStandardFile!=0 ? *poStandardFile : (LPCTSTR)0),	// default file
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		oFilter);
	if (oOpenDialog.DoModal()==IDOK)
	{
		// the file has been selected
		ReleaseStandardFile();
		m_poCurrentStandardFile=new CString(oOpenDialog.GetPathName());
		LoadJobList(*m_poCurrentStandardFile);
	}
	else
	{
		// user abort or error
		if (CommDlgExtendedError()!=0)
		{
			// an error occured
			AfxMessageBox(IDS_ErrorDuringFileSelection);
		}
	}

	//UpdateData(FALSE);
}

void CFaac_winguiDlg::OnButtonDuplicateSelected() 
{
	// TODO: Add your control notification handler code here

	CJobList *poJobList=GetGlobalJobList();

	TItemList<long> oCurSelection(CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, true));
	if (oCurSelection.GetNumber()==0)
	{
		// error message corresponds to last argument of the CWindowUtil::GetAll[...] call
		// currently we don't display an error message if no item is selected
		return;
	}
	TItemList<long> oNewSelection(oCurSelection);
	//TItemList<long> oNewSelection;		// enable this line instead of the previous one if you don't want to keep the previously selected items selected

	CBListReader oReader(oCurSelection);
	long lCurIndex;
	while (oCurSelection.GetNextElemContent(oReader, lCurIndex))
	{
		// get the current job
		CJob *poJob=poJobList->GetJob(lCurIndex);
		long lNewIndex=poJobList->AddJob(*poJob);
		oNewSelection.AddNewElem(lNewIndex);
	}

	// update the list control
	{
		CListCtrlStateSaver oNewSelectionState;
		oNewSelectionState.SetToSelected(oNewSelection);
		ReFillInJobListCtrl(&oNewSelectionState, false);

		// the selection may have changed, so make sure the property
		// dialog is updated
		OnJobListCtrlUserAction();
	}
}

void CFaac_winguiDlg::OnButtonProcessAll() 
{
	// TODO: Add your control notification handler code here
	if (!UpdateData(TRUE)) return;

	ProcessJobs(GetGlobalJobList()->GetAllUsedIds(), m_bCheckRemoveProcessedJobs!=0);
}

void CFaac_winguiDlg::ProcessJobs(TItemList<long> oJobIds, bool bRemoveProcessJobs)
{
	if (oJobIds.GetNumber()==0) return;
	CJobList *poJobList=GetGlobalJobList();

	// disable the properties window; important - otherwise the user
	// could change selected jobs while they are being processed
	CPropertiesDummyParentDialog *poPropertiesDialog=((CFaac_winguiApp*)AfxGetApp())->GetGlobalPropertiesDummyParentDialogSingleton();
	poPropertiesDialog->EnableWindow(FALSE);

	CListCtrlStateSaver oListCtrlState(&m_ctrlListJobs);

#ifdef EXTENDED_JOB_TRACKING_DURING_PROCESSING
	{
		TItemList<long> oSelectedItems(oJobIds);
		CWindowUtil::SetListCtrlCheckBoxStyle(&m_ctrlListJobs, true);
		CListCtrlStateSaver oTempCheckBoxState;
		oTempCheckBoxState.SetToChecked(oSelectedItems);
		oTempCheckBoxState.RestoreState(&m_ctrlListJobs);
	}
#endif

	TItemList<long> oItemsToRemove;

	long lTotalNumberOfJobsToProcess=oJobIds.GetNumber();
	long lCurJobCount=0;
	long lSuccessfulJobs=0;
	CJobProcessingDynamicUserInputInfo oUserInputInfo;	// saves dynamic input of the user

	CBListReader oReader(oJobIds);
	long lCurIndex;
	bool bContinue=true;
	while (oJobIds.GetNextElemContent(oReader, lCurIndex))
	{
		// only mark the currently processed job in the list control
		{
			CListCtrlStateSaver oSelection;
			oSelection.SetToSelected(lCurIndex);
			oSelection.RestoreState(&m_ctrlListJobs);
			int iCurItem=CWindowUtil::GetListCtrlItemIdByLParam(&m_ctrlListJobs, lCurIndex);
			if (iCurItem>=0)
			{
				m_ctrlListJobs.EnsureVisible(iCurItem, FALSE);
			}
		}

		// get the current job
		CJob *poJob=poJobList->GetJob(lCurIndex);
		poJob->SetJobNumberInfo(lCurJobCount++, lTotalNumberOfJobsToProcess);

		if (bContinue)
		{
			if (!poJob->ProcessJob(oUserInputInfo))
			{
				// show changes in process states
				ReFillInJobListCtrl(0, true);
				if (poJob->GetProcessingOutcomeSimple()==CAbstractJob::eUserAbort)
				{
					if (AfxMessageBox(IDS_ErrorProcessingJobSelection, MB_YESNO)!=IDYES)
					{
						bContinue=false;
					}
				}
			}
			else
			{
				// successfully processed
				lSuccessfulJobs++;
				if (bRemoveProcessJobs)
				{
					int iListItem=CWindowUtil::GetListCtrlItemIdByLParam(&m_ctrlListJobs, lCurIndex);
					ASSERT(iListItem>=0);		// must exist (logically)
					m_ctrlListJobs.DeleteItem(iListItem);

					oItemsToRemove.AddNewElem(lCurIndex);
				}
				else
				{
					// show changes in process states
					ReFillInJobListCtrl(0, true);
				}
			}
		}
		else
		{
			// mark job as aborted

			// cast is necessary here because CJob masks the required overload
			// due to overriding another one
			((CAbstractJob*)poJob)->SetProcessingOutcome(CAbstractJob::eUserAbort, 0, IDS_FollowUp);
		}
	}

#ifdef EXTENDED_JOB_TRACKING_DURING_PROCESSING
	{
		CWindowUtil::SetListCtrlCheckBoxStyle(&m_ctrlListJobs, false);
	}
#endif

	oListCtrlState.RestoreState(&m_ctrlListJobs);

	if (oItemsToRemove.GetNumber()>0)
	{
		OnJobListCtrlUserAction();	// this call is very important since the property dialog must be informed about the new selection before the underlying jobs are deleted
		poJobList->DeleteElems(oItemsToRemove);
		ReFillInJobListCtrl(0, false);

		// the selection may have changed, so make sure the property
		// dialog is updated
		OnJobListCtrlUserAction();
	}
	else
	{
		// show changes in process states
		ReFillInJobListCtrl(0, true);
	}

	// wake up the user
	MessageBeep(MB_OK);

	if (lSuccessfulJobs<lTotalNumberOfJobsToProcess)
	{
		AfxMessageBox(IDS_HadUnsuccessfulJobs);
	}

	// reenable the properties window we disabled at the beginning
	poPropertiesDialog->EnableWindow(TRUE);
}

bool CFaac_winguiDlg::LoadJobList(const CString &oCompletePath)
{
	// the file has been selected
	CFileSerializableJobList oJobList;
	if (!oJobList.LoadFromFile(oCompletePath, 1))
	{
		AfxMessageBox(IDS_ErrorWhileLoadingJobList, MB_OK | MB_ICONSTOP);
		return false;
	}
	else
	{
		// first make sure the property window doesn't contain anything
		CWindowUtil::DeleteAllListCtrlItems(&m_ctrlListJobs);
		OnJobListCtrlUserAction();

		*GetGlobalJobList()=oJobList;
		ReFillInJobListCtrl(0, false);
		return true;
	}
}

bool CFaac_winguiDlg::SaveJobList(const CString &oCompletePath)
{
	// the file has been selected
	CFileSerializableJobList oJobList(*GetGlobalJobList());
	if (!oJobList.SaveToFile(oCompletePath, 1))
	{
		AfxMessageBox(IDS_ErrorWhileSavingJobList, MB_OK | MB_ICONSTOP);
		return false;
	}
	else
	{
		return true;
	}
}

void CFaac_winguiDlg::OnButtonOpenProperties() 
{
	// TODO: Add your control notification handler code here
	ShowPropertiesWindow();
}

void CFaac_winguiDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);
	
	// TODO: Add your message handler code here
	
}

void CFaac_winguiDlg::OnButtonExpandFilterJob() 
{
	// TODO: Add your control notification handler code here
	TItemList<long> oCurSelection(CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, false));

	long lSelectedJobId;
	long lDummy;
	oCurSelection.GetFirstElemContent(lSelectedJobId, lDummy);

	CJobList *poGlobalJobList=GetGlobalJobList();
	CJob *poJob=poGlobalJobList->GetJob(lSelectedJobId);
	CJobList oExpanded;
	long lExpandErrors=0;
	if (poJob->GetEncoderJob()==0)
	{
		ASSERT(false);
		return;
	}
	else if (poJob->GetEncoderJob()->ExpandFilterJob(oExpanded, lExpandErrors, false))
	{
		int iListItemToDelete=CWindowUtil::GetListCtrlItemIdByLParam(&m_ctrlListJobs, lSelectedJobId);
		if (iListItemToDelete>=0)
		{
			m_ctrlListJobs.DeleteItem(iListItemToDelete);
			OnJobListCtrlUserAction();	// this call is very important since the property dialog must be informed about the new selection before the underlying jobs are deleted
		}
		poGlobalJobList->DeleteElem(lSelectedJobId);
		*poGlobalJobList+=oExpanded;

		CListCtrlStateSaver oListSelection;
		oListSelection.SetToSelected(oExpanded.GetAllUsedIds());
		ReFillInJobListCtrl(&oListSelection, false);
		OnJobListCtrlUserAction();		// again: very important call
	}
}

void CFaac_winguiDlg::OnButtonAddFilterEncoderJob() 
{
	// TODO: Add your control notification handler code here
	CJobList *poJobList=GetGlobalJobList();

	TItemList<long> m_oIndicesOfAddedItems;

	// select a directory
	CString oCaption;
	oCaption.LoadString(IDS_SelectSourceDirDlgCaption);
	CFolderDialog oFolderDialog(this, oCaption);
	if (oFolderDialog.DoModal()==IDOK)
	{
		CFilePathCalc::MakePath(oFolderDialog.m_oFolderPath);
	}
	else
	{
		if (oFolderDialog.m_bError)
		{
			AfxMessageBox(IDS_ErrorDuringDirectorySelection);
		}
		return;
	}
	CString oSourceFileExt;
	oSourceFileExt.LoadString(IDS_EndSourceFileStandardExtension);
	
	// create a new job for the current file
	CEncoderJob oNewJob;
	GetGlobalProgramSettings()->ApplyToJob(oNewJob);

	oNewJob.GetFiles().SetSourceFileDirectory(oFolderDialog.m_oFolderPath);
	oNewJob.GetFiles().SetSourceFileName(CString("*.")+oSourceFileExt);
	//oNewJob.GetFiles().SetTargetFileDirectory(oTargetDir);		// already set by GetGlobalProgramSettings()->ApplyToJob(oNewJob);
	oNewJob.GetFiles().SetTargetFileName("");

	long lNewIndex=poJobList->AddJob(oNewJob);
	m_oIndicesOfAddedItems.AddNewElem(lNewIndex);
	
	CListCtrlStateSaver oSelection;
	oSelection.SetToSelected(m_oIndicesOfAddedItems);
	ReFillInJobListCtrl(&oSelection, false);

	m_ctrlListJobs.SetFocus();

	OnJobListCtrlUserAction();
}

void CFaac_winguiDlg::OnButtonAddEmptyEncoderJob() 
{
	// TODO: Add your control notification handler code here
	CJobList *poJobList=GetGlobalJobList();

	TItemList<long> m_oIndicesOfAddedItems;
	
	// create a new job for the current file
	CEncoderJob oNewJob;
	//GetGlobalProgramSettings()->ApplyToJob(oNewJob);

	oNewJob.GetFiles().SetSourceFileDirectory("");
	oNewJob.GetFiles().SetSourceFileName("");
	oNewJob.GetFiles().SetTargetFileDirectory("");
	oNewJob.GetFiles().SetTargetFileName("");

	long lNewIndex=poJobList->AddJob(oNewJob);
	m_oIndicesOfAddedItems.AddNewElem(lNewIndex);
	
	CListCtrlStateSaver oSelection;
	oSelection.SetToSelected(m_oIndicesOfAddedItems);
	ReFillInJobListCtrl(&oSelection, false);

	m_ctrlListJobs.SetFocus();

	OnJobListCtrlUserAction();
}
