#ifndef registry_h
#define registry_h

class CRegistry 
{
public:
			CRegistry();
			~CRegistry();

	BOOL	openReg(HKEY hKey, char *SubKey);
	BOOL	openCreateReg(HKEY hKey, char *SubKey);
	void	closeReg();
	void	DeleteRegVal(char *SubKey);
	void	DeleteRegKey(char *SubKey);

	void	setRegBool(char *keyStr , BOOL val);
	void	setRegByte(char *keyStr , BYTE val);
	void	setRegWord(char *keyStr , WORD val);
	void	setRegDword(char *keyStr , DWORD val);
	void	setRegFloat(char *keyStr , float val);
	void	setRegStr(char *keyStr , char *valStr);
	void	setRegValN(char *keyStr , BYTE *addr,  DWORD size);

	BOOL	getSetRegBool(char *keyStr, BOOL var);
	BYTE	getSetRegByte(char *keyStr, BYTE var);
	WORD	getSetRegWord(char *keyStr, WORD var);
	DWORD	getSetRegDword(char *keyStr, DWORD var);
	float	getSetRegFloat(char *keyStr, float var);
	int		getSetRegStr(char *keyStr, char *tempString, char *dest, int maxLen);
	int		getSetRegValN(char *keyStr, BYTE *tempAddr, BYTE *addr, DWORD size);

	HKEY	regKey;
	char	*path;
};
#endif