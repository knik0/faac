// FileSerializableJobList.cpp: implementation of the CFileSerializableJobList class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "FileSerializableJobList.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

CFileSerializableJobListElem::CFileSerializableJobListElem()
{
}

CFileSerializableJobListElem::~CFileSerializableJobListElem()
{
}

bool CFileSerializableJobListElem::PutToArchive(CArchive &oArchive, long lSaveParams) const
{
	switch (lSaveParams)
	{
	case 1:
		{
			return m_oJob.PutToArchive(oArchive);
		}
	default:
		{
			// unknown file format version
			return false;
		}
	}
}

bool CFileSerializableJobListElem::GetFromArchive(CArchive &oArchive, long lLoadParams)
{
	switch (lLoadParams)
	{
	case 1:
		{
			return m_oJob.GetFromArchive(oArchive);
		}
	default:
		{
			// unknown file format version
			return false;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFileSerializableJobList::CFileSerializableJobList()
{
}

CFileSerializableJobList::CFileSerializableJobList(CJobList &oJobList)
{
	GetFromRegularJobList(oJobList);
}

CFileSerializableJobList::~CFileSerializableJobList()
{

}

CFileSerializableJobList::operator CJobList()
{
	CJobList oRegularJobList;
	CopyToRegularJobList(oRegularJobList);
	return oRegularJobList;
}

long CFileSerializableJobList::AddJob(const CJob &oJob, long lDesiredIndex)
{
	CFileSerializableJobListElem *poNewElem=(CFileSerializableJobListElem*)Add(lDesiredIndex);
	poNewElem->m_oJob=oJob;
	return poNewElem->GetIndex();
}

CJob* CFileSerializableJobList::GetJob(long lIndex)
{
	CFileSerializableJobListElem *poElem=(CFileSerializableJobListElem*)GetElem(lIndex);
	if (poElem==0) return 0;
	return &poElem->m_oJob;
}

CJob* CFileSerializableJobList::GetNextJob(CBListReader &oReader, long *plIndex)
{
	CFileSerializableJobListElem *poElem=(CFileSerializableJobListElem*)ReadElem(oReader);
	if (poElem==0) return 0;
	return &poElem->m_oJob;
}

PBBaseElem CFileSerializableJobList::CreateElem()
{
	return new CFileSerializableJobListElem;
}

void CFileSerializableJobList::GetFromRegularJobList(CJobList &oJobList)
{
	DeleteAll();

	CBListReader oReader(oJobList);
	CJob *poCurJob;
	long lCurJobIndex;
	while ((poCurJob=oJobList.GetNextJob(oReader, &lCurJobIndex))!=0)
	{
		AddJob(*poCurJob, lCurJobIndex);
	}
}

void CFileSerializableJobList::CopyToRegularJobList(CJobList &oJobList)
{
	oJobList.DeleteAll();

	CBListReader oReader(*this);
	CJob *poCurJob;
	long lCurJobIndex;
	while ((poCurJob=GetNextJob(oReader, &lCurJobIndex))!=0)
	{
		oJobList.AddJob(*poCurJob, lCurJobIndex);
	}
}