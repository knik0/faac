// JobListCtrlDescribable.h: interface for the CJobListCtrlDescribable class.
// Author: Torsten Landmann
//
// Classes should implement this interface if they want to be used as jobs
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_JOBLISTCTRLDESCRIBABLE_H__DFE38E73_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_JOBLISTCTRLDESCRIBABLE_H__DFE38E73_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CJobListCtrlDescribable
{
public:
	CJobListCtrlDescribable();
	virtual ~CJobListCtrlDescribable();

	virtual CString DescribeJobTypeShort() const=0;
	virtual CString DescribeJobTypeLong() const=0;
	virtual CString DescribeJob() const=0;
};

#endif // !defined(AFX_JOBLISTCTRLDESCRIBABLE_H__DFE38E73_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
