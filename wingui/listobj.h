// list class
// Copyright 1999, Torsten Landmann
// successfully compiled and tested under MSVC 6.0
//
// revised 2001 by Torsten Landmann
// successfully compiled and tested under MSVC 6.0
//
//////////////////////////////////////////////////////

#ifndef __Listobj_h__
#define __Listobj_h__

#include "stdafx.h"

typedef signed long TBListErr;

const long iMaxNumberOfCBListItems=100000000;
const TBListErr siListNoErr=0;
const TBListErr siListErr=-1;		// error codes
const TBListErr siListSavErr=-2;
const long lMaxNumRS=256;			// maximum number of reading sessions

// public use of this type is obsolete; use CBListReader instead
typedef signed long HRead;

// objects of this class can be compared with a moving pointer to
// a certain element in a list; it's used during sequential reading
// of lists;
// this class wraps HRead and automatically finishes the read
// session once an instance becomes destructed
class CBListReader
{
	friend class CBList;

public:
	// simplest way to get a reader
	CBListReader(CBList &oList);

	// copy constructor
	CBListReader(const CBListReader &oSource);

	virtual ~CBListReader();

	// makes this class compatible with former HRead
	operator HRead() const	{ return m_hRead; }

	CBListReader& operator=(const CBListReader &oRight);

	void EndReaderSession()		{ AbandonCurrentContent(); }

protected:
	CBListReader(CBList *poList, HRead hRead);

private:
	CBList *m_poList;
	HRead m_hRead;
	int *m_piUsageStack;

	void AbandonCurrentContent();
};

enum ESortMode
{
	eAscending,
	eDescending
};

class CBList;

class CBBaseElem
{
	friend class CBList;

public:
	class CBBaseElem *GetNext() const { return pNext; }
	class CBBaseElem *GetPrevious() const { return pPrevious; }
	long GetIndex() const { return lIndex; }
	class CBList *GetParentList() const { return pParentList; }

	// -1: parameter element is higher
	//  0: same
	//  1: parameter element is smaller
	// in this base class elements are sorted by their index
	virtual signed long IsHigher(class CBBaseElem *pElem);

protected:
	CBBaseElem();     // only allowed for derivations and for CBList (=friend)
	virtual ~CBBaseElem();

	// override these members if you want to put your list
	// to a stream or retreive it from there;
	// do never forget to call these ones if you override them
	virtual bool PutToArchive(CArchive &oArchive, long lSaveParams) const;
	virtual bool GetFromArchive(CArchive &oArchive, long lLoadParams);
private:
	long lIndex;
	class CBBaseElem *pNext, *pPrevious;
	class CBList *pParentList;
};

typedef CBBaseElem *PBBaseElem;

class CBList
{
public:
	CBList();
	virtual ~CBList();
	TBListErr DeleteAll();

	TBListErr DeleteElem(long lIndex);
	bool Exists(long lIndex) const;

	// number of elements; error: siListErr
	signed long GetNumber() const;
		
	// error: 0
	PBBaseElem GetElem(long lIndex) const;

	// returns the element that is at the 0-based position
	// that's given by parameter; always count from first Element
	// on; error: 0
	PBBaseElem GetPos(long lPos) const;
		
	// finds out the position of the element with the specified
	// index; siListErr in case of errors
	long GetIndexPos(long lIndex) const;
		
	// sort using method "IsHigher()"
		// error: siListErr
	TBListErr Sort(ESortMode eSortMode=eAscending);
		
	// returns old state
	bool SetSortBeforeReading(bool bNewValue);
		

	PBBaseElem GetFirst()			{ return pFirst; }
	PBBaseElem GetLast()			{ return pLast; }


	// *******************************************
	// using the following methods you get the elements in the order
	// in which they are in the list
	// if a call of SetSortBeforeReading(true) has taken place, the
	// list is sorted beforehands;
	// as long as reading sessions are open the list is locked
	// and no data can be changed!! this includes that you cannot
	// sort or change the sort-before-reading-state

	// initializes a new reading session
	// returns a handle to this reading session
	// list is sorted if set using method "setsortbeforereading()"
	// error: siListErr
	// maximum number of reading sessions is limited by constant
	// iMaxNumRS
	HRead BeginRead();		// obsolete; use CBListReader BeginReadEx() instead!
	CBListReader BeginReadEx();
		

	// error: 0
	PBBaseElem ReadElem(HRead hRead);

	// end reading session designated by hRead
	TBListErr EndRead(HRead hRead);

	// usually you do not need to call this method; you have the choice to
	// call oReader.EndReaderSession() instead or not explicitely ending the
	// sessiion at all;
	// explicitely ending the session might be useful if you know that the
	// parent list will be destructed before the reader;
	TBListErr EndRead(CBListReader &oReader);

	// performs calls the OnMapHandle() member exactly one time for
	// each of the list element in the order in which they are in
	// the list; if SetSortBeforeReading(true) has been done the list is
	// sorted;
	// the return value is true if each call of OnMapHandle() returned
	// true, false otherwise;
	// note that the first sentence in this comment isn't quite correct;
	// if OnMapHandle() returns false, the operation is not continued
	bool Map(long lTask, long lHint);


	// puts the whole list with all its data to a stream
	// or retreives it from there
	TBListErr PutToArchive(CArchive &oArchive, long lSaveParams=-1) const;
	TBListErr GetFromArchive(CArchive &oArchive, long lLoadParams=-1);

	// self explaining
	TBListErr SaveToFile(const char *lpszFileName, long lSaveParams=-1);
	TBListErr LoadFromFile(const char *lpszFileName, long lLoadParams=-1);

protected:
	// properly inserts a new element in the list
	// a pointer to it is given back;
	// the new element has an index between (including)
	// 1 and iMaxNumberOfCBListItems
	PBBaseElem Add();
	// this version tries to give the given index
	// to the new element, if possible; negative values and 0
	// are NOT used!
	PBBaseElem Add(long lDesiredIndex);

	// this member works together with the Map() member; look there for
	// further information
	virtual bool OnMapHandle(CBBaseElem *poElem, long lTask, long lHint);

	// the following method must be overriden by every
	// subclass returning an element of the type of the particular
	// subclass of PBBaseElem
	virtual PBBaseElem CreateElem()=0;

	// this method is called immediately before and element is
	// to be deleted via delete;
	// it does nothing here but you may override it to free
	// other variables the content is referring to
	// !!! EXTREMELY IMPORTANT: !!!
	// if you override this method you MUST call
	// DeleteAll() in the destructor of your class
	virtual void OnPreDeleteItem(PBBaseElem poElem);

private:
	// content of elements is not changed; their position in the list
	// is exchanged; error: siListErr
	TBListErr Exchange(PBBaseElem pOne, PBBaseElem pTwo);
		
	// Performs a bubble sort on the given interval
	// note that there is no validation checking (e.g. if
	// pLeft is really left of pRight)
	// (sorting algorithms like quicksort or heapsort are not
	// useful on lists. They work only good and fast on arrays,
	// so I decided to implement bubble sort)
	// pLeft and pRight refer to the pointers; a pNext-Pointer
	// always points right, so by only using pNext-Pointers on
	// pLeft you MUST reach pRight at some time; otherwise
	// the programm will be going to crash with an access
	// violation
	// return value siListErr: error (e.g. no element in list)
	TBListErr DoBubbleSort(PBBaseElem pLeft, PBBaseElem pRight, ESortMode eSortMode);
		

	// finds the lowest available index that can be given to
	// a new element; used by method "Add()"
	signed long GetSmallestIndex();

	// finds an open reading position; used by method BeginRead()
	signed long FindOpenReadPos() const;

	// finds the lowest element in the list; used by "DoBubbleSort()"
	PBBaseElem FindLowest(PBBaseElem pBeginFrom, PBBaseElem pGoTo, ESortMode eSortMode);

	PBBaseElem pFirst, pLast;

	PBBaseElem pReadPos[256];
	bool bReadEntryUsed[256];
	long lNumOfOpenReadingSessions;

	bool SortBeforeReading;

	// saves the last index number that has been given
	// to a new element
	long lCurrentIndex;
	     
};
typedef CBList *PBList;

#endif // #ifndef __Listobj_h__