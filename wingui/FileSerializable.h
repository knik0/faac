// FileSerializable.h: interface for the CFileSerializable class.
// Author: Torsten Landmann
//
// An interface for classes that can be serialized to or deserialized
// from a file stream.
// Also contains some utility methods for CArchive interaction.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILESERIALIZABLE_H__5FC5E381_1729_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FILESERIALIZABLE_H__5FC5E381_1729_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CFileSerializable  
{
public:
	CFileSerializable();
	virtual ~CFileSerializable();

	virtual bool PutToArchive(CArchive &oArchive) const=0;
	virtual bool GetFromArchive(CArchive &oArchive)=0;

	static void SerializeBool(CArchive &oArchive, bool bBool);
	static void DeSerializeBool(CArchive &oArchive, bool &bBool);
};

#endif // !defined(AFX_FILESERIALIZABLE_H__5FC5E381_1729_11D5_8402_0080C88C25BD__INCLUDED_)
