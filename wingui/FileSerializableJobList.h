// FileSerializableJobList.h: interface for the CFileSerializableJobList class.
// Author: Torsten Landmann
//
// This class was necessary to be able to reuse certain existing code for
// loading/saving lists. It can be used as a substitute to CJobList in most
// cases. Sometimes explicit casting may be necessary in one or the other
// direction.
// Instances of CFileSerializableJobList have the advantage that they are
// serializable via CBList members such as
// SaveToFile() or LoadFromFile().
// Instances of CFileSerializableJobList have the disadvantage that they aren't
// instances of TItemList<CJob> so they don't provide quite some functionality.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILESERIALIZABLEJOBLIST_H__5FC5E383_1729_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FILESERIALIZABLEJOBLIST_H__5FC5E383_1729_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "listobj.h"

class CFileSerializableJobListElem : public CBBaseElem
{
	friend class CFileSerializableJobList;

protected:
	CFileSerializableJobListElem();     // only allowed for derivations and for CBList (=friend)
	virtual ~CFileSerializableJobListElem();

	// override these members if you want to put your list
	// to a stream or retreive it from there;
	// do never forget to call these ones if you override them
	virtual bool PutToArchive(CArchive &oArchive, long lSaveParams) const;
	virtual bool GetFromArchive(CArchive &oArchive, long lLoadParams);
private:
	CJob m_oJob;
};

class CFileSerializableJobList : public CBList  
{
public:
	CFileSerializableJobList();
	CFileSerializableJobList(CJobList &oJobList);
	virtual ~CFileSerializableJobList();

	operator CJobList();

	long AddJob(const CJob &oJob, long lDesiredIndex=-1);
	CJob* GetJob(long lIndex);
	CJob* GetNextJob(CBListReader &oReader, long *plIndex=0);

private:
	// -- overridden from CBList
	// the following method must be overriden by every
	// subclass returning an element of the type of the particular
	// subclass of PBBaseElem
	virtual PBBaseElem CreateElem();

	void GetFromRegularJobList(CJobList &oJobList);
	void CopyToRegularJobList(CJobList &oJobList);
};


#endif // !defined(AFX_FILESERIALIZABLEJOBLIST_H__5FC5E383_1729_11D5_8402_0080C88C25BD__INCLUDED_)
