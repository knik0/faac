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

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

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
	m_poCurrentStandardFile(0)
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
		CWindowUtil::AddListCtrlColumn(&m_ctrlListJobs, 0, oTypeColumn, 0.1);
		CWindowUtil::AddListCtrlColumn(&m_ctrlListJobs, 1, oInfoColumn, 0.895);
		ListView_SetExtendedListViewStyleEx(m_ctrlListJobs, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
		ReFillInJobListCtrl();

		// create a floating property window
		CFloatingPropertyDialog::CreateFloatingPropertiesDummyParentDialog();

		// make sure the floating window is initialized properly
		OnJobListCtrlSelChange();
	}
	
	return TRUE;  // return TRUE  unless you set the focus to a control
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
		}
	}
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
	TItemList<long> m_oSelectedJobsIds;
	if (poNewSelection!=0)
	{
		m_oSelectedJobsIds=*poNewSelection;
	}
	else
	{
		m_oSelectedJobsIds=CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, true);
	}
	CJobList &oJobs=((CFaac_winguiApp*)AfxGetApp())->GetGlobalJobList();

	CPropertiesDummyParentDialog *poPropertiesDialog=((CFaac_winguiApp*)AfxGetApp())->GetGlobalPropertiesDummyParentDialogSingleton();
	if (poPropertiesDialog==0)
	{
		// nothing to do here
		return;
	}

	// ask them all which pages they support
	CBListReader oReader(m_oSelectedJobsIds);
	long lCurJobIndex;
	CSupportedPropertyPagesData* poSupportedPages=0;
	TItemList<CJob*> oSelectedJobs;
	while (m_oSelectedJobsIds.GetNextElemContent(oReader, lCurJobIndex))
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

	CJobList *poJobList=GetGlobalJobList();

	TItemList<long> oCurSelection(CWindowUtil::GetAllSelectedListCtrlItemLParams(&m_ctrlListJobs, false));

	TItemList<long> oItemsToRemove;

	CBListReader oReader(oCurSelection);
	long lCurIndex;
	bool bContinue=true;
	while (bContinue && oCurSelection.GetNextElemContent(oReader, lCurIndex))
	{
		// get the current job
		CJob *poJob=poJobList->GetJob(lCurIndex);

		if (!poJob->ProcessJob())
		{
			if (AfxMessageBox(IDS_ErrorProcessingJobSelection, MB_YESNO)!=IDYES)
			{
				bContinue=false;
			}
		}
		else
		{
			// successfully processed
			if (m_bCheckRemoveProcessedJobs)
			{
				int iListItem=CWindowUtil::GetListCtrlItemIdByLParam(&m_ctrlListJobs, lCurIndex);
				ASSERT(iListItem>=0);		// must exist (logically)
				m_ctrlListJobs.DeleteItem(iListItem);

				oItemsToRemove.AddNewElem(lCurIndex);
			}
		}
	}

	if (oItemsToRemove.GetNumber()>0)
	{
		OnJobListCtrlUserAction();	// this call is very important since the property dialog must be informed about the new selection before the underlying jobs are deleted
		poJobList->DeleteElems(oItemsToRemove);
		ReFillInJobListCtrl(0, false);

		// the selection may have changed, so make sure the property
		// dialog is updated
		OnJobListCtrlUserAction();
	}
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
		CFileSerializableJobList oJobList(*GetGlobalJobList());
		ReleaseStandardFile();
		m_poCurrentStandardFile=new CString(oOpenDialog.GetPathName());
		if (!oJobList.SaveToFile(oOpenDialog.GetPathName(), 1))
		{
			AfxMessageBox(IDS_ErrorWhileSavingJobList, MB_OK | MB_ICONSTOP);
		}
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
		CFileSerializableJobList oJobList;
		ReleaseStandardFile();
		m_poCurrentStandardFile=new CString(oOpenDialog.GetPathName());
		if (!oJobList.LoadFromFile(oOpenDialog.GetPathName(), 1))
		{
			AfxMessageBox(IDS_ErrorWhileSavingJobList, MB_OK | MB_ICONSTOP);
		}
		else
		{
			*GetGlobalJobList()=oJobList;
			ReFillInJobListCtrl(0, false);
		}
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
