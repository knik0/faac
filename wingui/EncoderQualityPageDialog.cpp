// EncoderQualityPageDialog.cpp : implementation file
//

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderQualityPageDialog.h"
#include "EncoderQualityPropertyPageContents.h"
#include "WindowUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEncoderQualityPageDialog dialog


CEncoderQualityPageDialog::CEncoderQualityPageDialog(
	const TItemList<CJob*> &oJobsToConfigure,
	CJobListUpdatable *poListContainer, 
	CWnd* pParent /*=NULL*/):
	m_bInitialized(false),
	m_oJobsToConfigure(oJobsToConfigure),
	m_poListContainer(poListContainer),
	m_bIgnoreUpdates(false)
{
	//{{AFX_DATA_INIT(CEncoderQualityPageDialog)
	m_oEditBandwidth = _T("");
	m_oEditBitRate = _T("");
	m_iRadioAacProfile = 1;
	m_iRadioMpegVersion = 0;
	//}}AFX_DATA_INIT

	Create(CEncoderQualityPageDialog::IDD, pParent);
}

CEncoderQualityPageDialog::~CEncoderQualityPageDialog()
{
	UpdateJobs(true, true);
}


void CEncoderQualityPageDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEncoderQualityPageDialog)
	DDX_Control(pDX, IDC_CHECKUSETNS, m_ctrlCheckUseTns);
	DDX_Control(pDX, IDC_CHECKUSELTP, m_ctrlCheckUseLtp);
	DDX_Control(pDX, IDC_EDITBANDWIDTH, m_ctrlEditBandwidth);
	DDX_Control(pDX, IDC_EDITBITRATE, m_ctrlEditBitRate);
	DDX_Control(pDX, IDC_CHECKUSELFE, m_ctrlCheckUseLfe);
	DDX_Control(pDX, IDC_CHECKMIDSIDE, m_ctrlCheckMidSide);
	DDX_Text(pDX, IDC_EDITBANDWIDTH, m_oEditBandwidth);
	DDX_Text(pDX, IDC_EDITBITRATE, m_oEditBitRate);
	DDX_Radio(pDX, IDC_RADIOMPEGVERSION2, m_iRadioMpegVersion);
	DDX_Radio(pDX, IDC_RADIOAACPROFILEMAIN, m_iRadioAacProfile);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEncoderQualityPageDialog, CDialog)
	//{{AFX_MSG_MAP(CEncoderQualityPageDialog)
	ON_EN_UPDATE(IDC_EDITBITRATE, OnUpdateEditBitRate)
	ON_EN_UPDATE(IDC_EDITBANDWIDTH, OnUpdateEditBandwidth)
	ON_EN_KILLFOCUS(IDC_EDITBANDWIDTH, OnKillfocusEditBandWidth)
	ON_EN_KILLFOCUS(IDC_EDITBITRATE, OnKillfocusEditBitRate)
	ON_BN_CLICKED(IDC_CHECKMIDSIDE, OnCheckMidSide)
	ON_BN_CLICKED(IDC_CHECKUSELFE, OnCheckUseLfe)
	ON_BN_CLICKED(IDC_CHECKUSELTP, OnCheckUseLtp)
	ON_BN_CLICKED(IDC_CHECKUSETNS, OnCheckUseTns)
	ON_BN_CLICKED(IDC_RADIOAACPROFILELC, OnRadioAacProfileLc)
	ON_BN_CLICKED(IDC_RADIOAACPROFILEMAIN, OnRadioAacProfileMain)
	ON_BN_CLICKED(IDC_RADIOAACPROFILESSR, OnRadioAacProfileSsr)
	ON_BN_CLICKED(IDC_RADIOAACPROFILELTP, OnRadioAacProfileLtp)
	ON_BN_CLICKED(IDC_RADIOMPEGVERSION2, OnRadioMpegVersion2)
	ON_BN_CLICKED(IDC_RADIOMPEGVERSION4, OnRadioMpegVersion4)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEncoderQualityPageDialog message handlers

BOOL CEncoderQualityPageDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_bInitialized=true;

	// show our contents
	ApplyPageContents(ParseJobs());

	if (IsDlgButtonChecked(IDC_RADIOMPEGVERSION2) == BST_CHECKED)
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_RADIOAACPROFILELTP), FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CEncoderQualityPageDialog::OnUpdateEditBitRate() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_UPDATE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	CWindowUtil::ForceNumericContent(&m_ctrlEditBitRate, false);
}

void CEncoderQualityPageDialog::OnUpdateEditBandwidth() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_UPDATE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	CWindowUtil::ForceNumericContent(&m_ctrlEditBandwidth, false);
}


bool CEncoderQualityPageDialog::GetPageContents(CEncoderQualityPropertyPageContents &oTarget)
{
	if (!UpdateData(TRUE)) return false;

	oTarget.m_oBitRate.SetContent(m_oEditBitRate);
	oTarget.m_oBandwidth.SetContent(m_oEditBandwidth);
	oTarget.m_oAllowMidSide.SetCheckCode(m_ctrlCheckMidSide.GetCheck());
	oTarget.m_oUseTns.SetCheckCode(m_ctrlCheckUseTns.GetCheck());
	oTarget.m_oUseLtp.SetCheckCode(m_ctrlCheckUseLtp.GetCheck());
	oTarget.m_oUseLfe.SetCheckCode(m_ctrlCheckUseLfe.GetCheck());
	oTarget.m_oAacProfile.GetFromRadioGroupVariable(m_iRadioAacProfile, 4);
	oTarget.m_oMpegVersion.GetFromRadioGroupVariable(m_iRadioMpegVersion, 2);

	return true;
}

void CEncoderQualityPageDialog::ApplyPageContents(const CEncoderQualityPropertyPageContents &oPageContents)
{
	// disabled since it could cause error messages - we're overwriting everything anyway
	//UpdateData(TRUE);

	m_oEditBitRate=oPageContents.m_oBitRate.GetContent();
	m_oEditBandwidth=oPageContents.m_oBandwidth.GetContent();
	oPageContents.m_oAllowMidSide.ApplyCheckCodeToButton(&m_ctrlCheckMidSide);
	oPageContents.m_oUseTns.ApplyCheckCodeToButton(&m_ctrlCheckUseTns);
	oPageContents.m_oUseLtp.ApplyCheckCodeToButton(&m_ctrlCheckUseLtp);
	oPageContents.m_oUseLfe.ApplyCheckCodeToButton(&m_ctrlCheckUseLfe);
	oPageContents.m_oAacProfile.ApplyToRadioGroupVariable(m_iRadioAacProfile);
	oPageContents.m_oMpegVersion.ApplyToRadioGroupVariable(m_iRadioMpegVersion);

	if (m_bInitialized)
	{
		UpdateData(FALSE);
	}
}

CEncoderQualityPropertyPageContents CEncoderQualityPageDialog::ParseJobs()
{
	CEncoderQualityPropertyPageContents oToReturn;
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
			oToReturn=poEncoderJob->GetQualityPageContents();
			bFirstRun=false;
		}
		else
		{
			oToReturn*=poEncoderJob->GetQualityPageContents();
		}
	}

	return oToReturn;
}

void CEncoderQualityPageDialog::ModifyJobs(const CEncoderQualityPropertyPageContents &oPageContents)
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
		poEncoderJob->ApplyQualityPageContents(oPageContents);
	}
}

void CEncoderQualityPageDialog::UpdateJobs(bool bFinishCheckBoxSessions, bool bDlgDestructUpdate)
{
	if (::IsWindow(*this) && !m_bIgnoreUpdates)
	{
		CEncoderQualityPropertyPageContents oPageContents;
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

void CEncoderQualityPageDialog::OnKillfocusEditBandWidth() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderQualityPageDialog::OnKillfocusEditBitRate() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderQualityPageDialog::OnCheckMidSide() 
{
	// TODO: Add your control notification handler code here
	ProcessCheckBoxClick(&m_ctrlCheckMidSide, eAllowMidSide);
}

void CEncoderQualityPageDialog::OnCheckUseLfe() 
{
	// TODO: Add your control notification handler code here
	ProcessCheckBoxClick(&m_ctrlCheckUseLfe, eUseLfe);
}

void CEncoderQualityPageDialog::ProcessCheckBoxClick(CButton *poCheckBox, ETypeOfCheckBox eTypeOfCheckBox)
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

void CEncoderQualityPageDialog::FinishCurrentCheckBoxSessionIfNecessary()
{
	switch (m_eCurCheckBox)
	{
	case eAllowMidSide:
		{
			FinishCheckBoxSessionIfNecessary(&m_ctrlCheckMidSide);
			break;
		}
	case eUseTns:
		{
			FinishCheckBoxSessionIfNecessary(&m_ctrlCheckUseTns);
			break;
		}
	case eUseLtp:
		{
			FinishCheckBoxSessionIfNecessary(&m_ctrlCheckUseLtp);
			break;
		}
	case eUseLfe:
		{
			FinishCheckBoxSessionIfNecessary(&m_ctrlCheckUseLfe);
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

void CEncoderQualityPageDialog::FinishCheckBoxSessionIfNecessary(CButton *poCheckBox)
{
	int iCurCheck=poCheckBox->GetCheck();
	if (iCurCheck<2)
	{
		poCheckBox->SetButtonStyle(BS_AUTOCHECKBOX);
	}
}

void CEncoderQualityPageDialog::OnCheckUseLtp() 
{
	// TODO: Add your control notification handler code here
	ProcessCheckBoxClick(&m_ctrlCheckUseLtp, eUseLtp);
}

void CEncoderQualityPageDialog::OnCheckUseTns() 
{
	// TODO: Add your control notification handler code here
	ProcessCheckBoxClick(&m_ctrlCheckUseTns, eUseTns);
}

void CEncoderQualityPageDialog::OnRadioAacProfileLc() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderQualityPageDialog::OnRadioAacProfileMain() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderQualityPageDialog::OnRadioAacProfileSsr() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderQualityPageDialog::OnRadioAacProfileLtp() 
{
	// TODO: Add your control notification handler code here
	UpdateJobs();
}

void CEncoderQualityPageDialog::OnRadioMpegVersion2() 
{
	// LTP option is unavailable
	if (IsDlgButtonChecked(IDC_RADIOAACPROFILELTP) == BST_CHECKED)
	{
		CheckDlgButton(IDC_RADIOAACPROFILELTP, BST_UNCHECKED);
		CheckDlgButton(IDC_RADIOAACPROFILELC, BST_CHECKED);
	}
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_RADIOAACPROFILELTP), FALSE);
	UpdateJobs();
}

void CEncoderQualityPageDialog::OnRadioMpegVersion4()
{
	// LTP option is available
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_RADIOAACPROFILELTP), TRUE);
	UpdateJobs();
}
