// ConcreteJobBase.h: interface for the CConcreteJobBase class.
// Author: Torsten Landmann
//
// just a helper to simplify the class hierarchy
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONCRETEJOBBASE_H__442115CF_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_CONCRETEJOBBASE_H__442115CF_0FD4_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "JobListCtrlDescribable.h"
#include "AbstractJob.h"
#include "FileSerializable.h"

class CConcreteJobBase : public CJobListCtrlDescribable,  public CAbstractJob,
							public CFileSerializable
{
public:
	CConcreteJobBase();
	virtual ~CConcreteJobBase();

};

#endif // !defined(AFX_CONCRETEJOBBASE_H__442115CF_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
