//#include "stdafx.h"
#include <windows.h>
//#include <memory.h>
#include "Registry.h"

registryClass::registryClass()
{
	regKey=NULL;
	path=NULL;
}

registryClass::~registryClass()
{
	if(regKey)
		RegCloseKey(regKey);
	if(path)
		delete path;
}

#define setPath(SubKey) \
	if(path) \
		delete path; \
	path=strdup(SubKey);

// *****************************************************************************

BOOL registryClass::openCreateReg(HKEY hKey, char *SubKey)
{
	if(regKey)
		RegCloseKey(regKey);
	if(RegOpenKeyEx(hKey, SubKey, NULL , KEY_ALL_ACCESS , &regKey)==ERROR_SUCCESS)
	{
		setPath(SubKey);
		return TRUE;
	}
	else // open failed -> create the key
	{
	DWORD disp;
		RegCreateKeyEx(hKey , SubKey, NULL , NULL, REG_OPTION_NON_VOLATILE , KEY_ALL_ACCESS , NULL , &regKey , &disp );
		if(disp==REG_CREATED_NEW_KEY) 
		{
			setPath(SubKey);
			return TRUE;
		}
		else
		{
			setPath("");
			return FALSE;
		}
	}
}
// *****************************************************************************

void registryClass::setRegDword(char *keyStr , DWORD val)
{
DWORD tempVal;
DWORD len;
	if(RegQueryValueEx(regKey , keyStr , NULL , NULL, (BYTE *)&tempVal , &len )!=ERROR_SUCCESS ||
		tempVal!=val)
		RegSetValueEx(regKey , keyStr , NULL , REG_DWORD , (BYTE *)&val , sizeof(DWORD));
}
// *****************************************************************************

DWORD registryClass::getSetRegDword(char *keyStr, DWORD val)
{
DWORD tempVal;
DWORD len=sizeof(DWORD);

	if(RegQueryValueEx(regKey , keyStr , NULL , NULL, (BYTE *)&tempVal , &len )!=ERROR_SUCCESS)
	{
		RegSetValueEx(regKey , keyStr , NULL , REG_DWORD , (BYTE *)&val , sizeof(DWORD));
		return val;
	}
	return (DWORD)tempVal;
}
