// TItemList.h: Header file for class template
// TItemList;
// Copyright 2000 by Torsten Landmann
//
// with this template you can create
// lists that are able to hold elements of the
// specified type; each element gets a unique
// handle in the list that is returned on creation
// you can choose whatever type you want as content under
// one condition: the operators ==, =, < and > must be defined;
// however < and > do only need to return senseful results
// if you wish to use the sorting feature of TItemList<..>-Lists;
// == must always be reflexive and must never return
// true if the two elements aren't really the same C++-object or value;
// if all this comparison operator stuff is too complicated for
// you and you think you don't really need to pay attention to
// it for a specific class just derive this class from
// CGenericSortable; note however that even then the = operator
// still needs to be implemented by you!
//
// revised 2001 by Torsten Landmann
// successfully compiled and tested under MSVC 6.0
// 
/////////////////////////////////////////////////////////

#ifndef __TItemList_h__
#define __TItemList_h__


#include "ListObj.h"

template <class ItemType> class TItemList;

template <class ItemType> class TItemListElem:public CBBaseElem
{
	friend class TItemList<ItemType>;

protected:
	TItemListElem();
	virtual ~TItemListElem();

	void SetContent(const ItemType &etContent);
	bool GetContent(ItemType &etContent) const;
	bool GetContent(ItemType* &etContent);
	ItemType* GetContent();
	const ItemType* GetContent() const;

	virtual signed long IsHigher(class CBBaseElem *pElem);	// overridden; used for sorting the list

private:
	bool m_bContentDefined;
	ItemType m_etContent;
};


template <class LItemType> class TItemList:public CBList
{
public:
	TItemList();
	TItemList(const TItemList<LItemType> &oSource);
	virtual ~TItemList();

	// return value: unique handle of the new item; siListErr in
	//				case of errors
	// note: if lDesiredIndex is supplied and the index is
	//	1.: >0 and
	//	2.: not yet used as index
	// then you can be sure that it is used for the new element
	long AddNewElem(const LItemType &etContent, long lDesiredIndex=-1);

	bool DeleteElemByContent(const LItemType &etContent);	// deletes maximally one element
	void DeleteAllElemsWithContent(const LItemType &etContent);

	void DeleteElems(TItemList<long> &oIndices);

	bool GetElemContent(long lIndex, LItemType &etContent) const;
	bool GetElemContent(long lIndex, LItemType* &etContent) const;
	bool SetElemContent(long lIndex, const LItemType &etContent) const;
	bool GetPosContent(long lPos, LItemType &etContent, long *lIndex=0) const;	// lIndex may be used to get the index of the element
	bool GetPosContent(long lPos, LItemType* &etContent, long *lIndex=0) const;	// lIndex may be used to get the index of the element
	bool SetPosContent(long lPos, const LItemType &etContent) const;
	bool GetNextElemContent(HRead hRead, LItemType &etContent, long *lIndex=0);	// lIndex may be used to get the index of the element
	bool GetNextElemContent(HRead hRead, LItemType* &etContent, long *lIndex=0);	// lIndex may be used to get the index of the element

	bool FindContent(const LItemType &etContent) const;
	bool FindContent(const LItemType &etContent, long &lId) const;
	bool FindContent(const LItemType &etContent, long lStartSearchAfterId, long &lId) const;
	long GetContentCount(const LItemType &etContent) const;
	TItemList<long> GetContentIds(const LItemType &etContent) const;
	TItemList<long> GetAllUsedIds() const;

	bool GetFirstElemContent(LItemType &etContent, long &lId) const;
	bool GetFirstElemContent(LItemType* &etContent, long &lId) const;

	bool GetGreatestContent(LItemType &etContent, long *plId=0) const;
	bool GetGreatestContent(LItemType* &etContent, long *plId=0) const;
	bool GetSmallestContent(LItemType &etContent, long *plId=0) const;
	bool GetSmallestContent(LItemType* &etContent, long *plId=0) const;

	// you may also use operator= for this
	bool CopyFrom(const TItemList<LItemType> &oSource);

	// standard operators;
	// subtraction preserves the indices of elements while
	// addition does not necessarily!
	// however if you are sure that no index would be in the list for several
	// times after adding the other list also addition preserves the indices;
	// subtraction is not number sensitive, so one element may
	// remove several others from the target list;
	TItemList<LItemType>& operator=(const TItemList<LItemType> &oRight);
	TItemList<LItemType> operator+(const TItemList<LItemType> &oRight);
	TItemList<LItemType> operator-(const TItemList<LItemType> &oRight);
	TItemList<LItemType>& operator+=(const TItemList<LItemType> &oRight);
	TItemList<LItemType>& operator-=(const TItemList<LItemType> &oRight);

	// multiplication intersects the two lists
	TItemList<LItemType> operator*(const TItemList<LItemType> &oRight);
	TItemList<LItemType>& operator*=(const TItemList<LItemType> &oRight);
	
	

	// removes all elements that are twice or more often in the list;
	// relatively slow, use sparingly (time complexity O(n*n));
	// you may provide a faster implementation if needed using
	// an efficient sorting algorithm
	void RemoveDoubleElements();

protected:
	// the following method must be overriden by every
	// subclass returning an element of the type of the particular
	// subclass of PBBaseElem
	virtual PBBaseElem CreateElem();

	// this method may be overriden by any sub class to perform
	// certain operations on a content before it is removed
	// from the list (and maybe also the last possibility to
	// access the content)
	// for instance, if Content is a pointer, you might want
	// to call "delete Content;" before the pointer itself is
	// removed from the list;
	// !!! EXTREMELY IMPORTANT: !!!
	// if you override this method you MUST call
	// DeleteAll() in the destructor of your class
	virtual void OnPreDeleteContent(LItemType Content);

private:
	// this method is inherited and called each time 
	// an element is to be deleted;
	// it extracts its content and passes it on to a call
	// of OnPreDeleteContent()
	virtual void OnPreDeleteItem(PBBaseElem poElem);
};



// ---------------- template definitions -------------------------

template <class ItemType> TItemListElem<ItemType>::TItemListElem():
	CBBaseElem()
{
	m_bContentDefined=false;
}

template <class ItemType> TItemListElem<ItemType>::~TItemListElem()
{
}

template <class ItemType> void TItemListElem<ItemType>::SetContent(const ItemType &etContent)
{
	m_bContentDefined=true;
	m_etContent=etContent;
}

template <class ItemType> bool TItemListElem<ItemType>::GetContent(ItemType &etContent) const
{
	if (!m_bContentDefined) return false;
	etContent=m_etContent;
	return true;
}

template <class ItemType> bool TItemListElem<ItemType>::GetContent(ItemType* &etContent)
{
	if (!m_bContentDefined) return false;
	etContent=&m_etContent;
	return true;
}

template <class ItemType> ItemType* TItemListElem<ItemType>::GetContent()
{
	return &m_etContent;
}

template <class ItemType> const ItemType* TItemListElem<ItemType>::GetContent() const
{
	return &m_etContent;
}

template <class ItemType> signed long TItemListElem<ItemType>::IsHigher(class CBBaseElem *pElem)
{
	ItemType ParamContent;

	((TItemListElem<ItemType>*)pElem)->GetContent(ParamContent);
	
	if (ParamContent<m_etContent) return 1;
	if (ParamContent>m_etContent) return -1;
	return 0;
}



template <class LItemType> TItemList<LItemType>::TItemList():
	CBList()
{
}

template <class LItemType> TItemList<LItemType>::TItemList(const TItemList<LItemType> &oSource)
{
	CopyFrom(oSource);
}

template <class LItemType> TItemList<LItemType>::~TItemList()
{
}

template <class LItemType> long TItemList<LItemType>::AddNewElem(
	const LItemType &etContent, long lDesiredIndex)
{
	TItemListElem<LItemType>* poNewElem=
		(TItemListElem<LItemType>*)Add(lDesiredIndex);

	if (poNewElem==0)
	{
		return siListErr;
	}

	poNewElem->SetContent(etContent);
	return poNewElem->GetIndex();
}

template <class LItemType> bool TItemList<LItemType>::DeleteElemByContent(const LItemType &etContent)
{
	long lItemId;
	if (!FindContent(etContent, lItemId))
	{
		return false;
	}
	return DeleteElem(lItemId)!=siListErr;
}

template <class LItemType> void TItemList<LItemType>::DeleteAllElemsWithContent(const LItemType &etContent)
{
	while (DeleteElemByContent(etContent))
	{
		// nothing further
	}
}

template <class LItemType> void TItemList<LItemType>::DeleteElems(TItemList<long> &oIndices)
{
	HRead hRead=oIndices.BeginRead();
	long lCurIndex;
	while (oIndices.GetNextElemContent(hRead, lCurIndex))
	{
		DeleteElem(lCurIndex);
	}
	oIndices.EndRead(hRead);
}

template <class LItemType> bool TItemList<LItemType>::GetElemContent(
	long lIndex,
	LItemType &etContent) const
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)GetElem(lIndex);

	if (poElem==0) return false;

	return poElem->GetContent(etContent);
}

template <class LItemType> bool TItemList<LItemType>::GetElemContent(
	long lIndex,
	LItemType* &etContent) const
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)GetElem(lIndex);

	if (poElem==0) return false;

	return poElem->GetContent(etContent);
}

template <class LItemType> bool TItemList<LItemType>::SetElemContent(long lIndex, const LItemType &etContent) const
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)GetElem(lIndex);

	if (poElem==0) return false;

	poElem->SetContent(etContent);

	return true;
}

template <class LItemType> bool TItemList<LItemType>::GetPosContent(long lPos, LItemType &etContent, long *lIndex) const
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)GetPos(lPos);

	if (poElem==0) return false;

	if (lIndex!=0) *lIndex=poElem->GetIndex();

	return poElem->GetContent(etContent);
}

template <class LItemType> bool TItemList<LItemType>::GetPosContent(long lPos, LItemType* &etContent, long *lIndex) const
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)GetPos(lPos);

	if (poElem==0) return false;

	if (lIndex!=0) *lIndex=poElem->GetIndex();

	return poElem->GetContent(etContent);
}

template <class LItemType> bool TItemList<LItemType>::SetPosContent(long lPos, const LItemType &etContent) const
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)GetPos(lPos);

	if (poElem==0) return false;

	poElem->SetContent(etContent);

	return true;
}

template <class LItemType> bool TItemList<LItemType>::GetNextElemContent(HRead hRead, LItemType &etContent, long *lIndex)
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)ReadElem(hRead);

	if (poElem==0) return false;

	if (lIndex!=0)
	{
		*lIndex=poElem->GetIndex();
	}

	return poElem->GetContent(etContent);
}

template <class LItemType> bool TItemList<LItemType>::GetNextElemContent(
	HRead hRead, LItemType* &etContent, long *lIndex)
{
	TItemListElem<LItemType>* poElem=
		(TItemListElem<LItemType>*)ReadElem(hRead);

	if (poElem==0) return false;

	if (lIndex!=0)
	{
		*lIndex=poElem->GetIndex();
	}

	return poElem->GetContent(etContent);
}

template <class LItemType> bool TItemList<LItemType>::FindContent(const LItemType &etContent) const
{
	long lDummy;
	return FindContent(etContent, lDummy);
}

template <class LItemType> bool TItemList<LItemType>::FindContent(const LItemType &etContent, long &lId) const
{
	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// loop through all elements and compare them with the
	// given one
	LItemType etReadContent;
	HRead hRead=poThis->BeginRead();
	while (poThis->GetNextElemContent(hRead, etReadContent, &lId))
	{
		if (etReadContent==etContent)
		{
			poThis->EndRead(hRead);
			return true;
		}
	}

	poThis->EndRead(hRead);
	return false;
}

template <class LItemType> bool TItemList<LItemType>::FindContent(const LItemType &etContent, long lStartSearchAfterId, long &lId) const
{
	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// loop through all elements and compare them with the
	// given one; however wait until we've passed the item
	// with id lStartSearchAfterId
	LItemType etReadContent;
	HRead hRead=poThis->BeginRead();
	bool bSearchBegun=false;
	while (poThis->GetNextElemContent(hRead, etReadContent, &lId))
	{
		if (etReadContent==etContent && bSearchBegun)
		{
			poThis->EndRead(hRead);
			return true;
		}

		if (lId==lStartSearchAfterId)
		{
			bSearchBegun=true;
		}
	}

	poThis->EndRead(hRead);
	return false;
}

template <class LItemType> long TItemList<LItemType>::GetContentCount(const LItemType &etContent) const
{
	long lCount=0;

	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// loop through all elements and compare them with the
	// given one
	LItemType etReadContent;
	HRead hRead=poThis->BeginRead();
	while (poThis->GetNextElemContent(hRead, etReadContent))
	{
		if (etReadContent==etContent)
		{
			lCount++;
		}
	}

	poThis->EndRead(hRead);
	return lCount;
}

template <class LItemType> TItemList<long> TItemList<LItemType>::GetContentIds(const LItemType &etContent) const
{
	TItemList<long> oReturn;

	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// loop through all elements and compare them with the
	// given one
	LItemType etReadContent;
	HRead hRead=poThis->BeginRead();
	long lCurId;
	while (poThis->GetNextElemContent(hRead, etReadContent, &lCurId))
	{
		if (etReadContent==etContent)
		{
			oReturn.AddNewElem(lCurId);
		}
	}

	poThis->EndRead(hRead);
	return oReturn;
}

template <class LItemType> TItemList<long> TItemList<LItemType>::GetAllUsedIds() const
{
	TItemList<long> oReturn;

	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// loop through all elements and collect their ids
	LItemType *etReadContent;		// just a dummy
	HRead hRead=poThis->BeginRead();
	long lCurId;
	while (poThis->GetNextElemContent(hRead, etReadContent, &lCurId))
	{
		oReturn.AddNewElem(lCurId);
	}

	poThis->EndRead(hRead);
	return oReturn;
}

template <class LItemType> bool TItemList<LItemType>::GetFirstElemContent(
	LItemType &etContent, long &lId) const
{
	// use another overloaded version
	LItemType *petContent;
	if (!GetFirstElemContent(petContent, lId))
	{
		return false;
	}

	etContent=*petContent;

	// return success
	return true;
}


template <class LItemType> bool TItemList<LItemType>::GetFirstElemContent(
	LItemType* &etContent, long &lId) const
{
	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// try to fetch the first element
	HRead hRead=poThis->BeginRead();
	if (poThis->GetNextElemContent(hRead, etContent, &lId))
	{
		// we could read an element
		poThis->EndRead(hRead);
		return true;
	}
	else
	{
		// we could read no element
		poThis->EndRead(hRead);
		return false;
	}
}

template <class LItemType> bool TItemList<LItemType>::GetGreatestContent(
	LItemType &etContent, long *plId) const
{
	// use another overloaded version
	LItemType *poContent;
	if (!GetGreatestContent(poContent, plId))
	{
		return false;
	}

	etContent=*poContent;
}

template <class LItemType> bool TItemList<LItemType>::GetGreatestContent(
	LItemType* &etContent, long *plId) const
{
	if (GetNumber()==0) return false;		// no element there

	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// loop through all elements and always save the greatest
	LItemType *petCurGreatestContent;
	long lCurGreatestIndex;
	if (!GetFirstElemContent(petCurGreatestContent, lCurGreatestIndex)) return false;

	HRead hRead=poThis->BeginRead();
	LItemType *petReadContent;
	long lCurIndex;
	while (poThis->GetNextElemContent(hRead, petReadContent, &lCurIndex))
	{
		if (*petReadContent>*petCurGreatestContent)
		{
			petCurGreatestContent=petReadContent;
			lCurGreatestIndex=lCurIndex;
		}
	}
	poThis->EndRead(hRead);

	// set return results
	etContent=petCurGreatestContent;
	if (plId!=0) *plId=lCurGreatestIndex;


	// return success
	return true;
}

template <class LItemType> bool TItemList<LItemType>::GetSmallestContent(
	LItemType &etContent, long *plId) const
{
	// use another overloaded version
	LItemType *poContent;
	if (!GetSmallestContent(poContent, plId))
	{
		return false;
	}

	etContent=*poContent;

	// return success
	return true;
}

template <class LItemType> bool TItemList<LItemType>::GetSmallestContent(
	LItemType* &etContent, long *plId) const
{
	if (GetNumber()==0) return false;		// no element there

	// we're using non-const members though we're going
	// to reverse all changes; therefore let's disable
	// the const checking
	TItemList<LItemType> *poThis=(TItemList<LItemType>*)this;

	// loop through all elements and always save a pointer to the smallest
	LItemType *petCurSmallestContent;
	long lCurSmallestIndex;
	if (!GetFirstElemContent(petCurSmallestContent, lCurSmallestIndex)) return false;

	HRead hRead=poThis->BeginRead();
	LItemType *petReadContent;
	long lCurIndex;
	while (poThis->GetNextElemContent(hRead, petReadContent, &lCurIndex))
	{
		if (*petReadContent<*petCurSmallestContent)
		{
			petCurSmallestContent=petReadContent;
			lCurSmallestIndex=lCurIndex;
		}
	}
	poThis->EndRead(hRead);

	// set return results
	etContent=petCurSmallestContent;
	if (plId!=0) *plId=lCurSmallestIndex;


	// return success
	return true;
}

template <class LItemType> bool TItemList<LItemType>::CopyFrom(
	const TItemList<LItemType> &oSource)
{
	ASSERT(&oSource!=this);

	// we will not have changed the state of oSource on return;
	// however we'll need non-const operations, so
	// allow ourselves to use non-const operations
	TItemList<LItemType> *poSource=(TItemList<LItemType>*)&oSource;

	// now begin the actual copying
	DeleteAll();

	HRead hRead=poSource->BeginRead();

	LItemType CurItem;
	long lCurItemIndex;
	while (poSource->GetNextElemContent(hRead, CurItem, &lCurItemIndex))
	{
		long lNewIndex=AddNewElem(CurItem, lCurItemIndex);
		ASSERT(lNewIndex==lCurItemIndex);	// assert that the copy got the same index as the source
	}

	poSource->EndRead(hRead);

	// return success
	return true;
}

template <class LItemType> TItemList<LItemType>& TItemList<LItemType>::operator=(const TItemList<LItemType> &oRight)
{
	CopyFrom(oRight);
	return *this;
}

template <class LItemType> TItemList<LItemType> TItemList<LItemType>::operator+(const TItemList<LItemType> &oRight)
{
	TItemList<LItemType> oReturn;

	// put our elements to return
	oReturn=*this;

	// put the rvalue's elements to return
	oReturn+=oRight;

	return oReturn;
}

template <class LItemType> TItemList<LItemType> TItemList<LItemType>::operator-(const TItemList<LItemType> &oRight)
{
	TItemList<LItemType> oReturn;

	// put our elements to return
	oReturn=*this;

	// remove the rvalue's elements from return
	oReturn-=oRight;

	return oReturn;
}

template <class LItemType> TItemList<LItemType>& TItemList<LItemType>::operator+=(const TItemList<LItemType> &oRight)
{
	// we will not have changed the state of oRight on return;
	// however we'll need non-const operations, so
	// allow ourselves to use non-const operations
	TItemList<LItemType> *poRight=(TItemList<LItemType>*)&oRight;

	// now begin the actual adding
	HRead hRead=poRight->BeginRead();

	LItemType CurItem;
	long lCurItemIndex;
	while (poRight->GetNextElemContent(hRead, CurItem, &lCurItemIndex))
	{
		AddNewElem(CurItem, lCurItemIndex);
	}

	poRight->EndRead(hRead);

	return *this;
}

template <class LItemType> TItemList<LItemType>& TItemList<LItemType>::operator-=(const TItemList<LItemType> &oRight)
{
	// go through all elements in the right list and delete them
	// from this one

	// we will not have changed the state of oRight on return;
	// however we'll need non-const operations, so
	// allow ourselves to use non-const operations
	TItemList<LItemType> *poRight=(TItemList<LItemType>*)&oRight;;

	HRead hRead=poRight->BeginRead();
	LItemType CurItem;
	while (poRight->GetNextElemContent(hRead, CurItem))
	{
		// delete the content from this list
		DeleteAllElemsWithContent(CurItem);
	}
	poRight->EndRead(hRead);

	return *this;
}

template <class LItemType> TItemList<LItemType>& TItemList<LItemType>::operator*=(const TItemList<LItemType> &oRight)
{
	// we save the ids of all items that are to be removed in this list;
	// this is necessary because the list is locked when read sessions are open
	TItemList<long> oIndicesToDelete;

	// remove all elements that are not in the oRight list
	HRead hRead=BeginRead();
	LItemType CurItem;
	long lCurItemIndex;
	while (GetNextElemContent(hRead, CurItem, &lCurItemIndex))
	{
		long lDummy;
		if (!oRight.FindContent(CurItem, lDummy))
		{
			// the item is not in the oRight list, so save
			// it for deletion
			oIndicesToDelete.AddNewElem(lCurItemIndex);
		}
	}
	EndRead(hRead);

	// now delete all items that we saved for deletion
	hRead=oIndicesToDelete.BeginRead();
	while (oIndicesToDelete.GetNextElemContent(hRead, lCurItemIndex))
	{
		DeleteElem(lCurItemIndex);
	}
	oIndicesToDelete.EndRead(hRead);

	return *this;
}

template <class LItemType> TItemList<LItemType> TItemList<LItemType>::operator*(const TItemList<LItemType> &oRight)
{
	TItemList<LItemType> oReturn;

	// put our elements to return
	oReturn=*this;

	// intersect the rvalue's elements with return's
	oReturn*=oRight;

	return oReturn;
}

template <class LItemType> void TItemList<LItemType>::RemoveDoubleElements()
{
	// we'll copy a copy of all distinct elements in this list
	TItemList<LItemType> oResultList;

	// go through all elements in this list and copy them
	// to the result list if appropriate
	HRead hRead=BeginRead();
	LItemType CurItem;
	while (GetNextElemContent(hRead, CurItem))
	{
		long lDummy;
		if (!oResultList.FindContent(CurItem, lDummy))
		{
			// the content is not yet in the list, so copy
			// it
			oResultList.AddNewElem(CurItem);
		}
	}
	EndRead(hRead);

	// now ResultList contains only distinct elements, so
	// copy it to ourselves
	*this=oResultList;
}



template <class LItemType> PBBaseElem TItemList<LItemType>::CreateElem()
{
	return new TItemListElem<LItemType>;
}

template <class LItemType> void TItemList<LItemType>::OnPreDeleteContent(LItemType Content)
{
	// do nothing here
}

template <class LItemType> void TItemList<LItemType>::OnPreDeleteItem(PBBaseElem poElem)
{
	// find out the content of the item
	LItemType Content;
	((TItemListElem<LItemType>*)poElem)->GetContent(Content);

	// inform sub classes of deletion of content
	OnPreDeleteContent(Content);
}

// see comment at the top of this file for a description
class CGenericSortable
{
public:
	CGenericSortable() {}
	virtual ~CGenericSortable() {}

	bool operator==(const CGenericSortable &oRight)
	{
		return this==&oRight;
	}

	bool operator<(const CGenericSortable &oRight)
	{
		return this<&oRight;
	}

	bool operator>(const CGenericSortable &oRight)
	{
		return &oRight<this;
	}
};


#endif // #ifndef __TItemList_h__