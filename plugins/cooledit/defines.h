#define APP_NAME "MPEG4-AAC"
#define APP_VER "v2.1"
#define REGISTRY_PROGRAM_NAME "SOFTWARE\\4N\\CoolEdit\\AAC-MPEG4"

// -----------------------------------------------------------------------------------------------

#define FREE(ptr) \
{ \
	if(ptr) \
		free(ptr); \
	ptr=0; \
}

// -----------------------------------------------------------------------------------------------

#define GLOBALLOCK(ptr,handle,type,ret) \
{ \
	if(!(ptr=(type *)GlobalLock(handle))) \
	{ \
		MessageBox(0, "GlobalLock", APP_NAME " plugin", MB_OK|MB_ICONSTOP); \
		ret; \
	} \
}
