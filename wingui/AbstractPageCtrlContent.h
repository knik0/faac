// AbstractPageCtrlContent.h: interface for the CAbstractPageCtrlContent class.
// Author: Torsten Landmann
//
// This is the base class for classes that are able to hold a piece
// of data from a property page; this is useful for "merging" the
// data of several selected jobs so that differing data isn't overwritten
// if not desired
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ABSTRACTPAGECTRLCONTENT_H__7B47B268_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ABSTRACTPAGECTRLCONTENT_H__7B47B268_0FF8_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CAbstractPageCtrlContent  
{
public:
	CAbstractPageCtrlContent();
	virtual ~CAbstractPageCtrlContent();

	bool Is3rdState() const					{ return m_b3rdStateContent; }
	void SetIs3rdState(bool b3rdState)		{ m_b3rdStateContent=b3rdState; }

	// all derived classes must implement this method;
	// it's important that identical hash strings always
	// mean identical content - also across class boundries;
	// that's why there is the following convention:
	// classes return a string <class name>-<A>
	// where <A> is the unique content in this object
	virtual CString GetHashString() const=0;

	// this operator connects the underlying objects so that
	// the 3rd state flag is correctly kept track of
	CAbstractPageCtrlContent& operator*=(const CAbstractPageCtrlContent &oRight);

private:
	bool m_b3rdStateContent;
};

#endif // !defined(AFX_ABSTRACTPAGECTRLCONTENT_H__7B47B268_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
