// list class
// Copyright 1999, Torsten Landmann
// successfully compiled and tested under MSVC 6.0
//
//////////////////////////////////////////////////////

#include "stdafx.h"
#include "Listobj.h"

// definitions for class CBListReader
CBListReader::CBListReader(CBList &oList)
{
	m_poList=&oList;
	m_hRead=oList.BeginRead();

	m_piUsageStack=new int;
	*m_piUsageStack=1;
}

CBListReader::CBListReader(const CBListReader &oSource):
	m_piUsageStack(0)
{
	*this=oSource;
}

CBListReader::~CBListReader()
{
	AbandonCurrentContent();
}


CBListReader& CBListReader::operator=(const CBListReader &oRight)
{
	if (this==&oRight) return *this;
	AbandonCurrentContent();

	m_poList=oRight.m_poList;
	m_hRead=oRight.m_hRead;
	m_piUsageStack=oRight.m_piUsageStack;
	if (m_piUsageStack!=0) *m_piUsageStack++;

	return *this;
}

CBListReader::CBListReader(CBList *poList, HRead hRead):
	 m_poList(poList), m_hRead(hRead)
{
	m_piUsageStack=new int;
	*m_piUsageStack=1;
}

void CBListReader::AbandonCurrentContent()
{
	if (m_piUsageStack!=0)
	{
		(*m_piUsageStack)--;
		if (*m_piUsageStack==0)
		{
			// last copy of this reader destroyed
			m_poList->EndRead(m_hRead);
			delete m_piUsageStack;
			m_piUsageStack=0;
		}
	}
}



// ----------- definitions for CBBaseElem ---------

CBBaseElem::CBBaseElem()
{
	lIndex=0;
	pNext=0;
	pPrevious=0;
}

CBBaseElem::~CBBaseElem()
{
}

signed long CBBaseElem::IsHigher(class CBBaseElem *pElem)
{
	if (lIndex>pElem->GetIndex())
	{
		return(1);
	}
	else
	{
		if (lIndex<pElem->GetIndex())
		{
			return(-1);
		}
		else
		{
			return(0);
		}
	}
}

bool CBBaseElem::PutToArchive(CArchive &oArchive, long lSaveParams) const
{
	oArchive << lIndex;

	return true;
}

bool CBBaseElem::GetFromArchive(CArchive &oArchive, long lLoadParams)
{
	oArchive >> lIndex;

	return true;
}


// ------ definitions for CBList ---------


CBList::CBList()
{
	pFirst=0; pLast=0;
	lCurrentIndex=1;
	SortBeforeReading=false;
	long lCount;
	for (lCount=lMaxNumRS-1; lCount>=0; lCount--)
	{
		bReadEntryUsed[lCount]=false;
	}
	lNumOfOpenReadingSessions=0;
}

CBList::~CBList()
{
	if (pFirst!=0)
	{
		DeleteAll();
	}
}

TBListErr CBList::DeleteAll()
{
	if (lNumOfOpenReadingSessions>0)
	{
#ifdef _DEBUG
		// release an error even in encapsulated code
		// (OCX controls and so on)
		char *pTest=0;
		char szTest=*pTest;
#endif
		return siListErr;
	}

	PBBaseElem pCurrent=pFirst, pDummy;
	while (pCurrent!=0)
	{
		pDummy=pCurrent->pNext;

		OnPreDeleteItem(pCurrent);	// inform sub classes of deletion
		pCurrent->pNext=0;
		delete pCurrent;

		pCurrent=pDummy;
	}
	pFirst=0; pLast=0;
	return siListNoErr;
}

signed long CBList::GetSmallestIndex()
   // determines the lowest available Index
{
	long lPos=lCurrentIndex+1;  // begin search at next element
	if (lPos>iMaxNumberOfCBListItems)
	{
		lPos=1;
	}
	while (Exists(lPos))
	{
		if (lPos==lCurrentIndex)  // one turn with nothing found?
		{
			return(siListErr);  // return error
		}
		if (lPos>iMaxNumberOfCBListItems)
		{
			lPos=1;
		}
		lPos++;
	}
	lCurrentIndex=lPos;
	return(lPos);
}



PBBaseElem CBList::Add()
{
	if (lNumOfOpenReadingSessions>0)
	{
#ifdef _DEBUG
		// release an error even in encapsulated code
		// (OCX controls and so on)
		char *pTest=0;
		char szTest=*pTest;
#endif
		return 0;
	}

	long lIndex=GetSmallestIndex();
	PBBaseElem pElem;
	if (lIndex==siListErr)
	{
		return 0;
	}

	pElem=CreateElem();


	if (pLast==0) pLast=pElem;
	pElem->pNext=pFirst;
	if (pFirst!=0) pElem->pNext->pPrevious=pElem;
	pElem->pPrevious=0;
	pFirst=pElem;
	pElem->lIndex=lIndex;
	pElem->pParentList=this;

	return pElem;
}

PBBaseElem CBList::Add(long lDesiredIndex)
{
	if (lNumOfOpenReadingSessions>0)
	{
#ifdef _DEBUG
		// release an error even in encapsulated code
		// (OCX controls and so on)
		char *pTest=0;
		char szTest=*pTest;
#endif
		return 0;
	}

	long lIndex=lDesiredIndex;

	if (lIndex<1 || lIndex>iMaxNumberOfCBListItems ||
		Exists(lIndex))
	{
		// the desired index can't be used
		return Add();
	}

	PBBaseElem pElem;

	pElem=CreateElem();


	if (pLast==0) pLast=pElem;
	pElem->pNext=pFirst;
	if (pFirst!=0) pElem->pNext->pPrevious=pElem;
	pElem->pPrevious=0;
	pFirst=pElem;
	pElem->lIndex=lIndex;
	pElem->pParentList=this;

	return pElem;
}

bool CBList::OnMapHandle(CBBaseElem *poElem, long lTask, long lHint)
{
	// do nothing; return false to avoid a waste of time
	return false;
}


// set to true virtual
/*PBBaseElem CBList::CreateElem()
{
	PBBaseElem pNewElem=new TBBaseElem;
	return pNewElem;
}*/

void CBList::OnPreDeleteItem(PBBaseElem poElem)
{
	// do nothing here
}

TBListErr CBList::DeleteElem(long lIndex)
{
	if (lNumOfOpenReadingSessions>0)
	{
#ifdef _DEBUG
		// release an error even in encapsulated code
		// (OCX controls and so on)
		char *pTest=0;
		char szTest=*pTest;
#endif
		return siListErr;
	}

	PBBaseElem pCurrent=pFirst;
	while (pCurrent!=0)
	{
		if (pCurrent->lIndex==lIndex)  // current corresponds to searched?
		{
			OnPreDeleteItem(pCurrent);	// inform sub class of deletion

			bool bDeleted=false;
			if (pCurrent==pFirst)   // first Element to be deleted?
			{
				pFirst=pFirst->pNext;
				if (pFirst!=0)
				{
					pFirst->pPrevious=0;
				}
				bDeleted=true;
			}
			if (pCurrent==pLast)  // last Element to be deleted?
			{
				pLast=pLast->pPrevious;
				if (pLast!=0)
				{
					pLast->pNext=0;
				}
				bDeleted=true;
			}
			if (!bDeleted)
				// neither first nor last element
			{
				pCurrent->pNext->pPrevious=pCurrent->pPrevious;
				pCurrent->pPrevious->pNext=pCurrent->pNext;
			}
			delete pCurrent;
			return siListNoErr;
		}
		pCurrent=pCurrent->pNext;
	}
	// if the programm reaches this point the desired element could
	// not be found
	return siListErr;
}


bool CBList::Exists(long lIndex) const
{
	PBBaseElem pCurrent=pFirst;
	while (pCurrent!=0)
	{
		if (pCurrent->lIndex==lIndex)  // desired element found?
		{
			return true;
		}
		pCurrent=pCurrent->pNext;
	}
	// if the program reaches this point the desired element could
	// not be found
	return false;
}

signed long CBList::GetNumber() const
{
	PBBaseElem pCurrent=pFirst;
	long lNumber=0;
	while (pCurrent!=0)
	{
		lNumber++;
		pCurrent=pCurrent->pNext;
	}
	return lNumber;
}


PBBaseElem CBList::GetElem(long lIndex) const
{
	PBBaseElem pCurrent=pFirst;
	while (pCurrent!=0)
	{
		if (pCurrent->lIndex==lIndex)
		{
			return pCurrent;
		}
		pCurrent=pCurrent->pNext;
	}

	// element was not found
	return 0;
}

PBBaseElem CBList::GetPos(long lPos) const
{
	PBBaseElem pCurrent=pFirst;
	while (pCurrent!=0)
	{
		if (lPos==0)
		{
			return pCurrent;
		}
		pCurrent=pCurrent->pNext;
		lPos--;
	}
	return 0;
}

long CBList::GetIndexPos(long lIndex) const
{
	long lCurPos=0;
	PBBaseElem pCurrent=pFirst;
	while (pCurrent!=0)
	{
		if (pCurrent->GetIndex()==lIndex)
		{
			return lCurPos;
		}

		pCurrent=pCurrent->pNext;
		lCurPos++;
	}

	// index was not found
	return siListErr;
}


TBListErr CBList::Sort(ESortMode eSortMode)
	// sort using methods "IsHigher()" and "Exchange()"
	// error: siListErr
{
	if (lNumOfOpenReadingSessions>0)
	{
#ifdef _DEBUG
		// release an error even in encapsulated code
		// (OCX controls and so on)
		char *pTest=0;
		char szTest=*pTest;
#endif
		return siListErr;
	}

	return DoBubbleSort(pFirst, pLast, eSortMode);
}

bool CBList::SetSortBeforeReading(bool bNewValue)
	// returns old state
{
	if (lNumOfOpenReadingSessions>0)
	{
#ifdef _DEBUG
		// release an error even in encapsulated code
		// (OCX controls and so on)
		char *pTest=0;
		char szTest=*pTest;
#endif
		return siListErr;
	}

	bool old=SortBeforeReading;
	SortBeforeReading=bNewValue;
	return old;
}

signed long CBList::FindOpenReadPos() const
{
	long lPos=0;
	while (bReadEntryUsed[lPos])
	{
		if (lPos>=lMaxNumRS)
		{
			return siListErr;
		}
		lPos++;
	}
	return lPos;
}

HRead CBList::BeginRead()
{
	HRead hNewReadingSession=FindOpenReadPos();
	if (hNewReadingSession==siListErr) return siListErr;

	if (SortBeforeReading) Sort();

	pReadPos[hNewReadingSession]=pFirst;
	bReadEntryUsed[hNewReadingSession]=true;
	lNumOfOpenReadingSessions++;
	return hNewReadingSession;
}

CBListReader CBList::BeginReadEx()
{
	return CBListReader(this, BeginRead());
}

PBBaseElem CBList::ReadElem(HRead hRead)
{
	PBBaseElem pCurrent=pReadPos[hRead];
	if (pReadPos[hRead]!=0)
		pReadPos[hRead]=pReadPos[hRead]->pNext;
	return pCurrent;
}

TBListErr CBList::EndRead(HRead hRead)
{
	bReadEntryUsed[hRead]=false;
	lNumOfOpenReadingSessions--;
	return siListNoErr;
}

TBListErr CBList::EndRead(CBListReader &oReader)
{
	oReader.EndReaderSession();
	return siListNoErr;
}

bool CBList::Map(long lTask, long lHint)
{
	HRead hRead=BeginRead();
	CBBaseElem *poCurElem;
	while ((poCurElem=ReadElem(hRead))!=0)
	{
		if (!OnMapHandle(poCurElem, lTask, lHint))
		{
			// action was aborted
			EndRead(hRead);
			return false;
		}
	}
	EndRead(hRead);

	// action was not aborted
	return true;
}


TBListErr CBList::PutToArchive(CArchive &oArchive, long lSaveParams) const
{
	// save the number of elements
	long lNumberOfElements=GetNumber();
	oArchive << lNumberOfElements;

	// go through all elements and save them
	PBBaseElem pCurrent=pFirst;
	while (pCurrent!=0)
	{
		if (!pCurrent->PutToArchive(oArchive, lSaveParams))
		{
			return siListErr;
		}

		pCurrent=pCurrent->pNext;
	}

	return siListNoErr;
}

TBListErr CBList::GetFromArchive(CArchive &oArchive, long lLoadParams)
{
	// remove old content
	DeleteAll();

	// get the number of elements
	long lNumberOfElements;
	oArchive >> lNumberOfElements;

	// go through all elements and load them
	while (lNumberOfElements>0)
	{
		// create a new element and fill it with data from the stream;
		// we give a desired index to avoid wasting time searching
		// for an available id
		PBBaseElem poCurElem;
		if (!(poCurElem=Add(23456))->GetFromArchive(oArchive, lLoadParams))
		{
			// return siListErr;	// I found this was too strict

			// delete the element that we've just created
			DeleteElem(poCurElem->GetIndex());
		}

		lNumberOfElements--;
	}

	return siListNoErr;
}

TBListErr CBList::SaveToFile(const char *lpszFileName, long lSaveParams)
{
	try
	{
		// open the file
		CFile oFile(lpszFileName, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite);
		CArchive oArchive(&oFile, CArchive::store);

		// put data to it
		return PutToArchive(oArchive, lSaveParams)==siListNoErr;
	}
	catch (...)
	{
		// error opening the file for exclusive writing
		return siListSavErr;
	}

}

TBListErr CBList::LoadFromFile(const char *lpszFileName, long lLoadParams)
{
	try
	{
		// open the file
		CFile oFile(lpszFileName, CFile::modeRead | CFile::shareDenyNone);
		CArchive oArchive(&oFile, CArchive::load);

		// get data from it
		return GetFromArchive(oArchive, lLoadParams)==siListNoErr;
	}
	catch (...)
	{
		// error opening the file for exclusive writing
		return siListErr;
	}
}

// ---- private definitions ----

TBListErr CBList::Exchange(PBBaseElem pOne, PBBaseElem pTwo)
{
	if (pOne==0 || pTwo==0) return siListErr;
	if (pOne==pTwo) return siListNoErr;

	PBBaseElem pBeforeOne=pOne->GetPrevious();
	PBBaseElem pBeforeTwo=pTwo->GetPrevious();
	PBBaseElem pAfterOne=pOne->GetNext();
	PBBaseElem pAfterTwo=pTwo->GetNext();

	// set new pFirst and pLast states, if necessary
	if (pOne==pFirst) pFirst=pTwo; else
		if (pTwo==pFirst) pFirst=pOne;
	if (pOne==pLast) pLast=pTwo; else
		if (pTwo==pLast) pLast=pOne;

	if (pBeforeTwo==pOne)
	{	// Elements follow on each other
		if (pOne->pPrevious!=0)	pOne->pPrevious->pNext=pTwo;
		if (pTwo->pNext!=0)		pTwo->pNext->pPrevious=pOne;

		pTwo->pPrevious=pOne->pPrevious;
		pOne->pPrevious=pTwo;

		pOne->pNext=pTwo->pNext;
		pTwo->pNext=pOne;
		return siListNoErr;
	}
	if (pBeforeOne==pTwo)
	{	// Elements follow on each other
		if (pTwo->pPrevious!=0)	pTwo->pPrevious->pNext=pOne;
		if (pOne->pNext!=0)		pOne->pNext->pPrevious=pTwo;

		pOne->pPrevious=pTwo->pPrevious;
		pTwo->pPrevious=pOne;

		pTwo->pNext=pOne->pNext;
		pOne->pNext=pTwo;
		return siListNoErr;
	}

	// if the execution comes to here both pOne and pTwo do not
	// follow each other

	if (pTwo->pNext!=0)		pTwo->pNext->pPrevious=pOne;
	if (pTwo->pPrevious!=0)	pTwo->pPrevious->pNext=pOne;
	if (pOne->pNext!=0)		pOne->pNext->pPrevious=pTwo;
	if (pOne->pPrevious!=0)	pOne->pPrevious->pNext=pTwo;
	
	PBBaseElem pDummy;

	pDummy=pOne->pNext;
	pOne->pNext=pTwo->pNext;
	pTwo->pNext=pDummy;

	pDummy=pOne->pPrevious;
	pOne->pPrevious=pTwo->pPrevious;
	pTwo->pPrevious=pDummy;

	return siListNoErr;
}

PBBaseElem CBList::FindLowest(PBBaseElem pBeginFrom, PBBaseElem pGoTo, ESortMode eSortMode)
{   // finds the element with the lowest key in a given interval
	if (pBeginFrom==pGoTo) return pBeginFrom;

	PBBaseElem pCurLowest=pBeginFrom;
	while (pBeginFrom!=pGoTo->GetNext())
	{
		if (eSortMode==eAscending)
		{
			if (pBeginFrom->IsHigher(pCurLowest)==-1)
				pCurLowest=pBeginFrom;
		}
		else
		{
			if (pBeginFrom->IsHigher(pCurLowest)==1)
				pCurLowest=pBeginFrom;
		}

		pBeginFrom=pBeginFrom->pNext;
	}
	return pCurLowest;
}


TBListErr CBList::DoBubbleSort(PBBaseElem pLeft, PBBaseElem pRight, ESortMode eSortMode)
{
	if (pLeft==0 || pRight==0) return siListNoErr;	// sucessfully finished
	if (pLeft==pRight) return siListNoErr;  // sucessfully finished

	

	PBBaseElem pLeftSortBorder=pLeft;
	PBBaseElem pLowestElem;
	while (pLeftSortBorder!=pRight)
	{
		pLowestElem=FindLowest(pLeftSortBorder, pRight, eSortMode);

		// exchanging the elements may have lead to a change of pRight
		if (pRight==pLowestElem) pRight=pLeftSortBorder;
		else if (pRight==pLeftSortBorder) pRight=pLowestElem;

		Exchange(pLowestElem, pLeftSortBorder);

		// pLowestElem's successor is now the new left sort border
		pLeftSortBorder=pLowestElem->pNext;
	}
	return siListNoErr;
}

