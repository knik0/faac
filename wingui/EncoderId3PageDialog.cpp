// EncoderId3PageDialog.cpp : implementation file
//

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderId3PageDialog.h"
#include "WindowUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEncoderId3PageDialog dialog


CEncoderId3PageDialog::CEncoderId3PageDialog(
	const TItemList<CJob*> &oJobsToConfigure,
	CJobListUpdatable *poListContainer,
	CWnd* pParent /*=NULL*/):
	m_bInitialized(false),
	m_oJobsToConfigure(oJobsToConfigure),
	m_poListContainer(poListContainer),
	m_eCurCheckBox(eNone),
	m_bIgnoreUpdates(false)
{
	//{{AFX_DATA_INIT(CEncoderId3PageDialog)
	m_oEditAlbum = _T("");
	m_oEditArtist = _T("");
	m_oEditComment = _T("");
	m_oEditComposer = _T("");
	m_oEditEncodedBy = _T("");
	m_oEditOriginalArtist = _T("");
	m_oEditTitle = _T("");
	m_oEditTrack = _T("");
	m_oEditUrl = _T("");
	m_oEditYear = _T("");
	//}}AFX_DATA_INIT

	Create(CEncoderId3PageDialog::IDD, pParent);
}

CEncoderId3PageDialog::~CEncoderId3PageDialog()
{
	UpdateJobs();
}


void CEncoderId3PageDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEncoderId3PageDialog)
	DDX_Control(pDX, IDC_EDITYEAR, m_ctrlEditYear);
	DDX_Control(pDX, IDC_EDITTRACK, m_ctrlEditTrack);
	DDX_Control(pDX, IDC_COMBOGENRE, m_ctrlComboGenre);
	DDX_Control(pDX, IDC_CHECKCOPYRIGHT, m_ctrlCheckCopyright);
	DDX_Text(pDX, IDC_EDITALBUM, m_oEditAlbum);
	DDX_Text(pDX, IDC_EDITARTIST, m_oEditArtist);
	DDX_Text(pDX, IDC_EDITCOMMENT, m_oEditComment);
	DDX_Text(pDX, IDC_EDITCOMPOSER, m_oEditComposer);
	DDX_Text(pDX, IDC_EDITENCODEDBY, m_oEditEncodedBy);
	DDX_Text(pDX, IDC_EDITORIGINALARTIST, m_oEditOriginalArtist);
	DDX_Text(pDX, IDC_EDITTITLE, m_oEditTitle);
	DDX_Text(pDX, IDC_EDITTRACK, m_oEditTrack);
	DDX_Text(pDX, IDC_EDITURL, m_oEditUrl);
	DDX_Text(pDX, IDC_EDITYEAR, m_oEditYear);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEncoderId3PageDialog, CDialog)
	//{{AFX_MSG_MAP(CEncoderId3PageDialog)
	ON_CBN_KILLFOCUS(IDC_COMBOGENRE, OnKillfocusComboGenre)
	ON_EN_KILLFOCUS(IDC_EDITALBUM, OnKillfocusEditAlbum)
	ON_EN_KILLFOCUS(IDC_EDITARTIST, OnKillfocusEditArtist)
	ON_EN_KILLFOCUS(IDC_EDITCOMMENT, OnKillfocusEditComment)
	ON_EN_KILLFOCUS(IDC_EDITCOMPOSER, OnKillfocusEditComposer)
	ON_EN_KILLFOCUS(IDC_EDITENCODEDBY, OnKillfocusEditEncodedBy)
	ON_EN_KILLFOCUS(IDC_EDITORIGINALARTIST, OnKillfocusEditOriginalArtist)
	ON_EN_KILLFOCUS(IDC_EDITTITLE, OnKillfocusEditTitle)
	ON_EN_KILLFOCUS(IDC_EDITTRACK, OnKillfocusEditTrack)
	ON_EN_KILLFOCUS(IDC_EDITURL, OnKillfocusEditUrl)
	ON_EN_KILLFOCUS(IDC_EDITYEAR, OnKillfocusEditYear)
	ON_BN_CLICKED(IDC_CHECKCOPYRIGHT, OnCheckCopyright)
	ON_EN_UPDATE(IDC_EDITTRACK, OnUpdateEditTrack)
	ON_EN_UPDATE(IDC_EDITYEAR, OnUpdateEditYear)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEncoderId3PageDialog message handlers

BOOL CEncoderId3PageDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_bInitialized=true;

	// show our contents
	ApplyPageContents(ParseJobs());
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

bool CEncoderId3PageDialog::GetPageContents(CEncoderId3PropertyPageContents &oTarget)
{
	if (!UpdateData(TRUE)) return false;

	oTarget.m_oArtist.SetContent(m_oEditArtist);
	oTarget.m_oTrackNo.SetContent(m_oEditTrack);
	oTarget.m_oAlbum.SetContent(m_oEditAlbum);
	oTarget.m_oYear.SetContent(m_oEditYear);
	oTarget.m_oTitle.SetContent(m_oEditTitle);
	oTarget.m_oCopyright.SetCheckCode(m_ctrlCheckCopyright.GetCheck());
	oTarget.m_oOriginalArtist.SetContent(m_oEditOriginalArtist);
	oTarget.m_oComposer.SetContent(m_oEditComposer);
	oTarget.m_oUrl.SetContent(m_oEditUrl);
	oTarget.m_oGenre.SetCurComboBoxSelectionText(&m_ctrlComboGenre);
	oTarget.m_oEncodedBy.SetContent(m_oEditEncodedBy);
	oTarget.m_oComment.SetContent(m_oEditComment);

	return true;
}

void CEncoderId3PageDialog::ApplyPageContents(const CEncoderId3PropertyPageContents &oPageContents)
{
	// disabled since it could cause error messages - we're overwriting everything anyway
	//UpdateData(TRUE);

	m_oEditArtist=oPageContents.m_oArtist.GetContent();
	m_oEditTrack=oPageContents.m_oTrackNo.GetContent();
	m_oEditAlbum=oPageContents.m_oAlbum.GetContent();
	m_oEditYear=oPageContents.m_oYear.GetContent();
	m_oEditTitle=oPageContents.m_oTitle.GetContent();
	oPageContents.m_oCopyright.ApplyCheckCodeToButton(&m_ctrlCheckCopyright);
	m_oEditOriginalArtist=oPageContents.m_oOriginalArtist.GetContent();
	m_oEditComposer=oPageContents.m_oComposer.GetContent();
	m_oEditUrl=oPageContents.m_oUrl.GetContent();
	oPageContents.m_oGenre.ApplyToComboBoxPointer(&m_ctrlComboGenre);
	m_oEditEncodedBy=oPageContents.m_oEncodedBy.GetContent();
	m_oEditComment=oPageContents.m_oComment.GetContent();

	if (m_bInitialized)
	{
		UpdateData(FALSE);
	}
}

CEncoderId3PropertyPageContents CEncoderId3PageDialog::ParseJobs()
{
	CEncoderId3PropertyPageContents oToReturn;
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
			oToReturn=poEncoderJob->GetId3PageContents();
			bFirstRun=false;
		}
		else
		{
			oToReturn*=poEncoderJob->GetId3PageContents();
		}
	}

	return oToReturn;
}

void CEncoderId3PageDialog::ModifyJobs(const CEncoderId3PropertyPageContents &oPageContents)
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
		poEncoderJob->ApplyId3PageContents(oPageContents);
	}
}

void CEncoderId3PageDialog::UpdateJobs(bool bFinishCheckBoxSessions, bool bDlgDestructUpdate)
{
	if (::IsWindow(*this) && !m_bIgnoreUpdates)
	{
		CEncoderId3PropertyPageContents oPageContents;
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

void CEncoderId3PageDialog::OnKillfocusComboGenre() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditAlbum() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditArtist() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditComment() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditComposer() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditEncodedBy() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditOriginalArtist() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditTitle() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditTrack() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditUrl() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnKillfocusEditYear() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderId3PageDialog::OnCheckCopyright() 
{
	// TODO: Add your control notification handler code here
	ProcessCheckBoxClick(&m_ctrlCheckCopyright, eCopyright);
}

void CEncoderId3PageDialog::ProcessCheckBoxClick(CButton *poCheckBox, ETypeOfCheckBox eTypeOfCheckBox)
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

void CEncoderId3PageDialog::FinishCurrentCheckBoxSessionIfNecessary()
{
	switch (m_eCurCheckBox)
	{
	case eCopyright:
		{
			FinishCheckBoxSessionIfNecessary(&m_ctrlCheckCopyright);
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

void CEncoderId3PageDialog::FinishCheckBoxSessionIfNecessary(CButton *poCheckBox)
{
	int iCurCheck=poCheckBox->GetCheck();
	if (iCurCheck<2)
	{
		poCheckBox->SetButtonStyle(BS_AUTOCHECKBOX);
	}
}

void CEncoderId3PageDialog::OnUpdateEditTrack() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_UPDATE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	CWindowUtil::ForceNumericContent(&m_ctrlEditTrack, false);
}

void CEncoderId3PageDialog::OnUpdateEditYear() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_UPDATE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	CWindowUtil::ForceNumericContent(&m_ctrlEditYear, false);
}
