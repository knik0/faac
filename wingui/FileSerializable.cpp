// FileSerializable.cpp: implementation of the CFileSerializable class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "FileSerializable.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFileSerializable::CFileSerializable()
{

}

CFileSerializable::~CFileSerializable()
{

}

void CFileSerializable::SerializeBool(CArchive &oArchive, bool bBool)
{
	int iBool=bBool ? 1 : 0;
	oArchive << iBool;
}

void CFileSerializable::DeSerializeBool(CArchive &oArchive, bool &bBool)
{
	int iBool;
	oArchive >> iBool;
	bBool=iBool!=0;
}