// Job.cpp: implementation of the CJob class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "Job.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CJob::CJob():
	m_eJobType(eUndefined),
	m_poJob(0)
{
}

CJob::CJob(const CEncoderJob &oEncoderJob):
	m_eJobType(eUndefined),
	m_poJob(0)
{
	SetEncoderJob(oEncoderJob);
}

CJob::CJob(const CJob &oSource):
	m_eJobType(eUndefined),
	m_poJob(0)
{
	*this=oSource;
}

CJob::~CJob()
{
	if (m_poJob!=0)
	{
		delete m_poJob;
	}
}


void CJob::SetEncoderJob(const CEncoderJob &oEncoderJob)
{
	ResetContent();

	m_eJobType=eEncoderJob;

	m_poJob=new CEncoderJob(oEncoderJob);
}

CJob& CJob::operator=(const CJob &oRight)
{
	if (this==&oRight) return *this;
	ResetContent();

	m_eJobType=oRight.m_eJobType;
	switch (m_eJobType)
	{
	case eEncoderJob:
		{
			m_poJob=new CEncoderJob(*((CEncoderJob*)oRight.m_poJob));
			break;
		}
	case eUndefined:
		{
			break;
		}
	default:
		{
			ASSERT(false);
			break;
		}
	}

	return *this;
}

CString CJob::DescribeJobTypeShort() const
{
	if (m_poJob!=0)
	{
		return m_poJob->DescribeJobTypeShort();
	}

	return "U";
}

CString CJob::DescribeJobTypeLong() const
{
	if (m_poJob!=0)
	{
		return m_poJob->DescribeJobTypeLong();
	}

	CString oUndefined;
	oUndefined.LoadString(IDS_UndefinedShort);
	return oUndefined;
}

CString CJob::DescribeJob() const
{
	if (m_poJob!=0)
	{
		return m_poJob->DescribeJob();
	}

	CString oInvalidJob;
	oInvalidJob.LoadString(IDS_InvalidJob);
	return oInvalidJob;
}

CSupportedPropertyPagesData CJob::GetSupportedPropertyPages() const
{
	if (m_poJob!=0)
	{
		return m_poJob->GetSupportedPropertyPages();
	}
	else
	{
		return CSupportedPropertyPagesData();
	}
}

CString CJob::GetDetailedDescriptionForStatusDialog() const
{
	// doesn't need an implementation here
	return "";
}

bool CJob::PutToArchive(CArchive &oArchive) const
{
	// put a class version flag
	int iVersion=1;
	oArchive << iVersion;

	oArchive << m_eJobType;

	bool bSerializeJob=m_poJob!=0;
	CFileSerializable::SerializeBool(oArchive, bSerializeJob);
	if (bSerializeJob)
	{
		if (!m_poJob->PutToArchive(oArchive)) return false;
	}

	return true;
}

bool CJob::GetFromArchive(CArchive &oArchive)
{
	// fetch the class version flag
	int iVersion;
	oArchive >> iVersion;

	switch (iVersion)
	{
	case 1:
		{
			oArchive >> (int&)m_eJobType;

			if (m_poJob!=0)
			{
				delete m_poJob;
				m_poJob=0;
			}
			bool bDeSerializeJob;
			CFileSerializable::DeSerializeBool(oArchive, bDeSerializeJob);
			if (bDeSerializeJob)
			{
				switch (m_eJobType)
				{
				case eEncoderJob:
					{
						m_poJob=new CEncoderJob;
						break;
					}
				default:
					{
						// unknown type of job
						ASSERT(false);
						return false;
					}
				}
				if (!m_poJob->GetFromArchive(oArchive)) return false;
			}
			else
			{
				m_poJob=0;
			}

			return true;
		}
	default:
		{
			// unknown file format version
			ASSERT(false);
			return false;
		}
	}
}

bool CJob::ProcessJob() const
{
	if (m_poJob!=0)
	{
		return m_poJob->ProcessJob();
	}
	else
	{
		return false;
	}
}

void CJob::ResetContent()
{
	if (m_poJob!=0)
	{
		delete m_poJob;
	}

	m_eJobType=eUndefined;
}
