// EncoderGeneralPageDialog.cpp : implementation file
//

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderGeneralPageDialog.h"
#include "EncoderGeneralPropertyPageContents.h"
#include "FolderDialog.h"
#include "FilePathCalc.h"
#include "FileMaskAssembler.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEncoderGeneralPageDialog dialog


CEncoderGeneralPageDialog::CEncoderGeneralPageDialog(
	const TItemList<CJob*> &oJobsToConfigure,
	CJobListUpdatable *poListContainer,
	CWnd* pParent /*=NULL*/):
	m_bInitialized(false),
	m_oJobsToConfigure(oJobsToConfigure),
	m_poListContainer(poListContainer),
	m_eCurCheckBox(eNone),
	m_bIgnoreUpdates(false)
{
	//{{AFX_DATA_INIT(CEncoderGeneralPageDialog)
	m_oEditSourceDir = _T("");
	m_oEditSourceFile = _T("");
	m_oEditTargetDir = _T("");
	m_oEditTargetFile = _T("");
	//}}AFX_DATA_INIT

	Create(CEncoderGeneralPageDialog::IDD, pParent);
}

CEncoderGeneralPageDialog::~CEncoderGeneralPageDialog()
{
	UpdateJobs();
}

void CEncoderGeneralPageDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEncoderGeneralPageDialog)
	DDX_Control(pDX, IDC_CHECKRECURSIVE, m_ctrlCheckRecursive);
	DDX_Control(pDX, IDC_BUTTONBROWSETARGETFILE, m_ctrlButtonBrowseTargetFile);
	DDX_Control(pDX, IDC_BUTTONBROWSESOURCEFILE, m_ctrlButtonBrowseSourceFile);
	DDX_Text(pDX, IDC_EDITSOURCEDIR, m_oEditSourceDir);
	DDX_Text(pDX, IDC_EDITSOURCEFILE, m_oEditSourceFile);
	DDX_Text(pDX, IDC_EDITTARGETDIR, m_oEditTargetDir);
	DDX_Text(pDX, IDC_EDITTARGETFILE, m_oEditTargetFile);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEncoderGeneralPageDialog, CDialog)
	//{{AFX_MSG_MAP(CEncoderGeneralPageDialog)
	ON_EN_KILLFOCUS(IDC_EDITSOURCEDIR, OnKillfocusEditSourceDir)
	ON_EN_KILLFOCUS(IDC_EDITSOURCEFILE, OnKillfocusEditSourceFile)
	ON_EN_KILLFOCUS(IDC_EDITTARGETDIR, OnKillfocusEditTargetDir)
	ON_EN_KILLFOCUS(IDC_EDITTARGETFILE, OnKillfocusEditTargetFile)
	ON_BN_CLICKED(IDC_BUTTONBROWSESOURCEDIR, OnButtonBrowseSourceDir)
	ON_BN_CLICKED(IDC_BUTTONBROWSETARGETDIR, OnButtonBrowseTargetDir)
	ON_BN_CLICKED(IDC_BUTTONBROWSESOURCEFILE, OnButtoBrowseSourceFile)
	ON_BN_CLICKED(IDC_BUTTONBROWSETARGETFILE, OnButtonBrowseTargetFile)
	ON_EN_CHANGE(IDC_EDITSOURCEFILE, OnChangeEditSourceFile)
	ON_BN_CLICKED(IDC_CHECKRECURSIVE, OnCheckRecursive)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEncoderGeneralPageDialog message handlers

BOOL CEncoderGeneralPageDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_bInitialized=true;

	if (m_oJobsToConfigure.GetNumber()>1)
	{
		// have several jobs
		//m_ctrlButtonBrowseSourceFile.ShowWindow(SW_HIDE);
		m_ctrlButtonBrowseTargetFile.ShowWindow(SW_HIDE);
	}

	// show our contents
	ApplyPageContents(ParseJobs());

	// not appropriate here (also ApplyPageContents() has done the job anyway)
	//OnChangeEditSourceFile();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

bool CEncoderGeneralPageDialog::GetPageContents(CEncoderGeneralPropertyPageContents &oTarget)
{
	if (!UpdateData(TRUE)) return false;

	oTarget.m_oSourceDirectory.SetContent(m_oEditSourceDir);
	oTarget.m_oSourceFile.SetContent(m_oEditSourceFile);
	oTarget.m_oTargetDirectory.SetContent(m_oEditTargetDir);
	oTarget.m_oTargetFile.SetContent(m_oEditTargetFile);
	oTarget.m_oSourceFileFilterIsRecursive.SetCheckCode(m_ctrlCheckRecursive.GetCheck());

	// not relevant
	oTarget.m_oSourceFilterRecursiveCheckboxVisible.SetIs3rdState(true);

	return true;
}

void CEncoderGeneralPageDialog::ApplyPageContents(const CEncoderGeneralPropertyPageContents &oPageContents)
{
	// disabled since it could cause error messages - we're overwriting everything anyway
	//UpdateData(TRUE);

	m_oEditSourceDir=oPageContents.m_oSourceDirectory.GetContent();
	m_oEditSourceFile=oPageContents.m_oSourceFile.GetContent();
	m_oEditTargetDir=oPageContents.m_oTargetDirectory.GetContent();
	m_oEditTargetFile=oPageContents.m_oTargetFile.GetContent();
	oPageContents.m_oSourceFileFilterIsRecursive.ApplyCheckCodeToButton(&m_ctrlCheckRecursive);

	if (m_bInitialized)
	{
		UpdateData(FALSE);

		if (m_oEditSourceFile.IsEmpty())
		{
			if (!oPageContents.m_oSourceFilterRecursiveCheckboxVisible.Is3rdState() &&
				oPageContents.m_oSourceFilterRecursiveCheckboxVisible.GetCheckMark())
			{
				m_ctrlCheckRecursive.ShowWindow(SW_SHOW);
			}
			else
			{
				m_ctrlCheckRecursive.ShowWindow(SW_HIDE);
			}
		}
		else
		{
			OnChangeEditSourceFile();
		}
	}
}

CEncoderGeneralPropertyPageContents CEncoderGeneralPageDialog::ParseJobs()
{
	CEncoderGeneralPropertyPageContents oToReturn;
	bool bFirstRun=true;

	CBListReader oReader(m_oJobsToConfigure);
	CJob *poCurJob;
	while (m_oJobsToConfigure.GetNextElemContent(oReader, poCurJob))
	{
		if (!poCurJob->GetJobType()==CJob::eEncoderJob)
		{
			// must all be encoder jobs
			ASSERT(false);
		}
		CEncoderJob *poEncoderJob=poCurJob->GetEncoderJob();
		if (bFirstRun)
		{
			oToReturn=poEncoderJob->GetGeneralPageContents();
			bFirstRun=false;
		}
		else
		{
			oToReturn*=poEncoderJob->GetGeneralPageContents();
		}
	}

	return oToReturn;
}

void CEncoderGeneralPageDialog::ModifyJobs(const CEncoderGeneralPropertyPageContents &oPageContents)
{
	CBListReader oReader(m_oJobsToConfigure);
	CJob *poCurJob;
	while (m_oJobsToConfigure.GetNextElemContent(oReader, poCurJob))
	{
		if (!poCurJob->GetJobType()==CJob::eEncoderJob)
		{
			// must all be encoder jobs
			ASSERT(false);
		}
		CEncoderJob *poEncoderJob=poCurJob->GetEncoderJob();
		poEncoderJob->ApplyGeneralPageContents(oPageContents);
	}
}

void CEncoderGeneralPageDialog::UpdateJobs(bool bFinishCheckBoxSessions, bool bDlgDestructUpdate)
{
	if (::IsWindow(*this) && !m_bIgnoreUpdates)
	{
		CEncoderGeneralPropertyPageContents oPageContents;
		if (GetPageContents(oPageContents))
		{
			if (bFinishCheckBoxSessions)
			{
				FinishCurrentCheckBoxSessionIfNecessary();
			}

			ModifyJobs(oPageContents);

			// make changes visible
			m_poListContainer->ReFillInJobListCtrl();
		}
	}

	if (bDlgDestructUpdate)
	{
		m_bIgnoreUpdates=true;
	}
}

void CEncoderGeneralPageDialog::OnKillfocusEditSourceDir() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderGeneralPageDialog::OnKillfocusEditSourceFile() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderGeneralPageDialog::OnKillfocusEditTargetDir() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderGeneralPageDialog::OnKillfocusEditTargetFile() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderGeneralPageDialog::OnButtonBrowseSourceDir() 
{
	CString oCaption;
	oCaption.LoadString(IDS_SelectSourceDirDlgCaption);
	CFolderDialog oFolderDialog(this, oCaption);
	if (oFolderDialog.DoModal()==IDOK)
	{
		UpdateData(TRUE);
		m_oEditSourceDir=oFolderDialog.m_oFolderPath;
		CFilePathCalc::MakePath(m_oEditSourceDir);
		UpdateData(FALSE);
		UpdateJobs();
	}
	else
	{
		if (oFolderDialog.m_bError)
		{
			AfxMessageBox(IDS_ErrorDuringDirectorySelection);
		}
	}
}

void CEncoderGeneralPageDialog::OnButtonBrowseTargetDir() 
{
	CString oCaption;
	oCaption.LoadString(IDS_SelectTargetDirDlgCaption);
	CFolderDialog oFolderDialog(this, oCaption);
	if (oFolderDialog.DoModal()==IDOK)
	{
		UpdateData(TRUE);
		m_oEditTargetDir=oFolderDialog.m_oFolderPath;
		CFilePathCalc::MakePath(m_oEditTargetDir);
		UpdateData(FALSE);
		UpdateJobs();
	}
	else
	{
		if (oFolderDialog.m_bError)
		{
			AfxMessageBox(IDS_ErrorDuringDirectorySelection);
		}
	}
}

void CEncoderGeneralPageDialog::OnButtoBrowseSourceFile() 
{
	UpdateData(TRUE);
	// determine the standard file
	CString *poStandardFile=0;
	if (!(m_oEditSourceFile.IsEmpty() || m_oEditSourceDir.IsEmpty()))
	{
		poStandardFile=new CString(m_oEditSourceDir+m_oEditSourceFile);
	}

	// assemble the filter string
	CString oFilter;
	{
		// assemble a list of allowed filters
		TItemList<int> oFilters;
		oFilters.AddNewElem(IDS_WavFilesFilter, 1);
		oFilters.AddNewElem(IDS_AllFilesFilter, 2);
		oFilter=CFileMaskAssembler::GetFileMask(oFilters);
	}

	// find out the default extension
	CString oDefaultExt;
	oDefaultExt.LoadString(IDS_EndSourceFileStandardExtension);

	CFileDialog oOpenDialog(
		TRUE,										// file open mode
		oDefaultExt,								// default extension
		(poStandardFile!=0 ? *poStandardFile : (LPCTSTR)0),	// default file
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		oFilter);
	if (oOpenDialog.DoModal()==IDOK)
	{
		// the file has been opened
		CString oFilePath(oOpenDialog.GetPathName());
		m_oEditSourceDir=oFilePath;
		CFilePathCalc::MakePath(m_oEditSourceDir);
		CFilePathCalc::ExtractFileName(oFilePath, m_oEditSourceFile);
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

	UpdateData(FALSE);
	UpdateJobs();
	OnChangeEditSourceFile();
}

void CEncoderGeneralPageDialog::OnButtonBrowseTargetFile() 
{
	UpdateData(TRUE);
	// determine the standard file
	CString *poStandardFile=0;
	if (!(m_oEditTargetFile.IsEmpty() || m_oEditTargetDir.IsEmpty()))
	{
		poStandardFile=new CString(m_oEditTargetDir+m_oEditTargetFile);
	}

	// assemble the filter string
	CString oFilter;
	{
		// assemble a list of allowed filters
		TItemList<int> oFilters;
		oFilters.AddNewElem(IDS_AacFilesFilter, 1);
		oFilters.AddNewElem(IDS_AllFilesFilter, 2);
		oFilter=CFileMaskAssembler::GetFileMask(oFilters);
	}

	// find out the default extension
	CString oDefaultExt;
	oDefaultExt.LoadString(IDS_EndTargetFileStandardExtension);

	CFileDialog oOpenDialog(
		FALSE,										// file save mode
		oDefaultExt,								// default extension
		(poStandardFile!=0 ? *poStandardFile : (LPCTSTR)0),	// default file
		OFN_HIDEREADONLY,
		oFilter);
	if (oOpenDialog.DoModal()==IDOK)
	{
		// the file has been opened
		CString oFilePath(oOpenDialog.GetPathName());
		m_oEditTargetDir=oFilePath;
		CFilePathCalc::MakePath(m_oEditTargetDir);
		CFilePathCalc::ExtractFileName(oFilePath, m_oEditTargetFile);
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

	UpdateData(FALSE);
	UpdateJobs();
	OnChangeEditSourceFile();
}


void CEncoderGeneralPageDialog::OnChangeEditSourceFile() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	// check for a filter
	if (CFilePathCalc::IsValidFileMask(m_oEditSourceFile))
	{
		// user entered a filter
		m_ctrlCheckRecursive.ShowWindow(SW_SHOW);
	}
	else
	{
		// user did not enter a filter
		m_ctrlCheckRecursive.ShowWindow(SW_HIDE);
	}

	UpdateData(FALSE);
}


void CEncoderGeneralPageDialog::ProcessCheckBoxClick(CButton *poCheckBox, ETypeOfCheckBox eTypeOfCheckBox)
{
	int iCheckState=poCheckBox->GetCheck();
	if (iCheckState==2)
	{
		// 3rd state
		if (m_eCurCheckBox!=eTypeOfCheckBox)
		{
			// must not be like this
			ASSERT(false);
		}
		else
		{
			m_oCheckStateChangeStateSaver.RestoreJobs(m_oJobsToConfigure);
			FinishCurrentCheckBoxSessionIfNecessary();
		}
	}
	else
	{
		if (m_eCurCheckBox!=eTypeOfCheckBox)
		{
			FinishCurrentCheckBoxSessionIfNecessary();
		
			// current checkbox is now set to eNone

			m_eCurCheckBox=eTypeOfCheckBox;

			m_oCheckStateChangeStateSaver.SaveJobs(m_oJobsToConfigure);
		}
	}

	UpdateJobs(false);
}

void CEncoderGeneralPageDialog::FinishCurrentCheckBoxSessionIfNecessary()
{
	switch (m_eCurCheckBox)
	{
	case eRecursive:
		{
			FinishCheckBoxSessionIfNecessary(&m_ctrlCheckRecursive);
			break;
		}
	case eNone:
		{
			// nothing
			break;
		}
	default:
		{
			// unkown type of check box
			break;
		}
	}
	m_eCurCheckBox=eNone;
}

void CEncoderGeneralPageDialog::FinishCheckBoxSessionIfNecessary(CButton *poCheckBox)
{
	int iCurCheck=poCheckBox->GetCheck();
	if (iCurCheck<2)
	{
		poCheckBox->SetButtonStyle(BS_AUTOCHECKBOX);
	}
}

void CEncoderGeneralPageDialog::OnCheckRecursive() 
{
	// TODO: Add your control notification handler code here
	ProcessCheckBoxClick(&m_ctrlCheckRecursive, eRecursive);
}
