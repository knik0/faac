#include <windows.h>
#include <stdio.h>


static char app_name[] = "Freeware AAC encoder";
static char INI_FILE[MAX_PATH];


static void _r_s(char *name,char *data, int mlen)
{
char buf[2048];
 strcpy(buf,data);
 GetPrivateProfileString(app_name,name,buf,data,mlen,INI_FILE);
}

#define RS(x) (_r_s(#x,x,sizeof(x)))
#define WS(x) (WritePrivateProfileString(app_name,#x,x,INI_FILE))


void config_init()
{
	char *p=INI_FILE;
	GetModuleFileName(NULL,INI_FILE,sizeof(INI_FILE));
	while (*p) p++;
	while (p >= INI_FILE && *p != '\\') p--;
	strcpy(p+1,"plugins.ini");
}

void config_read(DWORD *dwOptions)
{
char Options[512];
 config_init();
 RS(Options);
 *dwOptions=atoi(Options);
}

void config_write(DWORD dwOptions)
{
char Options[512];
 sprintf(Options,"%lu",dwOptions);
 WS(Options);
}
