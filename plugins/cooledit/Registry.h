#ifndef registry_h
#define registry_h

class registryClass 
{
public:
			registryClass();
			~registryClass();

	BOOL	openCreateReg(HKEY hKey, char *SubKey);
	void	setRegDword(char *keyStr , DWORD val);
	DWORD	getSetRegDword(char *keyStr, DWORD var);

	HKEY	regKey;
	char	*path;
};
#endif