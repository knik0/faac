/*
FAAC - codec plugin for Cooledit
Copyright (C) 2002 Antonio Foranna

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation.
	
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
		
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
			
The author can be contacted at:
kreel@tiscali.it
*/

#include <windows.h>
#include <stdio.h>		// FILE *
#include "resource.h"
#include "filters.h"	// CoolEdit
#include <faac.h>
#include <faad.h>
#include <mp4.h>
extern "C" {
#include <aacinfo.h>	// get_AAC_format()
}
#include "CRegistry.h"
#include "Defines.h"	// my defines
#include "Structs.h"	// my structs

// *********************************************************************************************

extern HINSTANCE hInst;

// -----------------------------------------------------------------------------------------------

extern BOOL DialogMsgProcAbout(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam);

// *********************************************************************************************

#define MAX_CHANNELS	2
#define	FAAD_STREAMSIZE	(FAAD_MIN_STREAMSIZE*MAX_CHANNELS)
#define RAW		0
#define ADIF	1
#define ADTS	2

// *********************************************************************************************

typedef struct input_tag // any special vars associated with input file
{
//MP4
MP4FileHandle	mp4File;
MP4SampleId		sampleId,
				numSamples;
int				track;
BYTE			type;

//AAC
FILE			*aacFile;
DWORD			src_size;		// size of compressed file
DWORD			tagsize;
DWORD			bytes_read;		// from file
DWORD			bytes_consumed;	// from buffer by faadDecDecode
long			bytes_into_buffer;
unsigned char	*buffer;

// Raw AAC
DWORD			bytesconsumed;	// to decode current frame by faadDecDecode

// GLOBAL
faacDecHandle	hDecoder;
faadAACInfo		file_info;
DWORD			len_ms;			// length of file in milliseconds
WORD			Channels;
DWORD			Samprate;
WORD			BitsPerSample;
DWORD			dst_size;		// size of decoded file. Cooledit needs it to update its progress bar
//char			*src_name;		// name of compressed file
bool			IsAAC;
} MYINPUT;
// -----------------------------------------------------------------------------------------------

static const char* mpeg4AudioNames[]=
{
	"Raw PCM",
	"AAC Main",
	"AAC Low Complexity",
	"AAC SSR",
	"AAC LTP",
	"Reserved",
	"AAC Scalable",
	"TwinVQ",
	"CELP",
	"HVXC",
	"Reserved",
	"Reserved",
	"TTSI",
	"Main synthetic",
	"Wavetable synthesis",
	"General MIDI",
	"Algorithmic Synthesis and Audio FX",
// defined in MPEG-4 version 2
	"ER AAC LC",
	"Reserved",
	"ER AAC LTP",
	"ER AAC Scalable",
	"ER TwinVQ",
	"ER BSAC",
	"ER AAC LD",
	"ER CELP",
	"ER HVXC",
	"ER HILN",
	"ER Parametric",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved"
};

// *********************************************************************************************

int id3v2_tag(unsigned char *buffer)
{
	if(StringComp((const char *)buffer, "ID3", 3) == 0)
	{
	unsigned long tagsize;

	// high bit is not used
		tagsize =	(buffer[6] << 21) | (buffer[7] << 14) |
					(buffer[8] <<  7) | (buffer[9] <<  0);
		tagsize += 10;
		return tagsize;
	}
	return 0;
}
// *********************************************************************************************

int GetAACTrack(MP4FileHandle infile)
{
// find AAC track
int i, rc;
int numTracks = MP4GetNumberOfTracks(infile, NULL, 0);

	for (i = 0; i < numTracks; i++)
    {
    MP4TrackId trackId = MP4FindTrackId(infile, i, NULL, 0);
    const char* trackType = MP4GetTrackType(infile, trackId);

        if (!strcmp(trackType, MP4_AUDIO_TRACK_TYPE))
        {
        unsigned char *buff = NULL;
        unsigned __int32 buff_size = 0;
        unsigned char dummy2_8, dummy3_8, dummy4_8, dummy5_8, dummy6_8,
            dummy7_8, dummy8_8;
        unsigned long dummy1_32;

			MP4GetTrackESConfiguration(infile, trackId, (unsigned __int8 **)&buff, &buff_size);

            if (buff)
            {
                rc = AudioSpecificConfig(buff, &dummy1_32, &dummy2_8, &dummy3_8, &dummy4_8,
                    &dummy5_8, &dummy6_8, &dummy7_8, &dummy8_8);
                free(buff);

                if (rc < 0)
                    return -1;
                return trackId;
            }
        }
    }

    // can't decode this
    return -1;
}
// *********************************************************************************************

void ReadCfgDec(MY_DEC_CFG *cfg) 
{ 
CRegistry reg;

	if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME  "\\FAAD"))
	{
		cfg->DecCfg.defObjectType=reg.getSetRegByte("Profile",LOW);
		cfg->DecCfg.defSampleRate=reg.getSetRegDword("SampleRate",44100);
		cfg->DecCfg.outputFormat=reg.getSetRegByte("Bps",FAAD_FMT_16BIT);
		cfg->Channels=reg.getSetRegByte("Channels",2);
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}
// -----------------------------------------------------------------------------------------------

void WriteCfgDec(MY_DEC_CFG *cfg)
{ 
CRegistry reg;

	if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME  "\\FAAD"))
	{
		reg.setRegByte("Profile",cfg->DecCfg.defObjectType); 
		reg.setRegDword("SampleRate",cfg->DecCfg.defSampleRate); 
		reg.setRegByte("Bps",cfg->DecCfg.outputFormat);
		reg.setRegByte("Channels",cfg->Channels);
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}
// -----------------------------------------------------------------------------------------------

//	EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_SSR), Enabled);
#define DISABLE_CTRL(Enabled) \
{ \
    EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MAIN), Enabled); \
    EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LOW), Enabled); \
    EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled); \
}
// -----------------------------------------------------------------------------------------------

static MY_DEC_CFG *CfgDecoder;

BOOL DialogMsgProcDecoder(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
	case WM_INITDIALOG:
		{
			if(!lParam)
			{
				MessageBox(hWndDlg,"Pointer==NULL",0,MB_OK|MB_ICONSTOP);
				EndDialog(hWndDlg, 0);
				return TRUE;
			}
			CfgDecoder=(MY_DEC_CFG *)lParam;

			switch(CfgDecoder->DecCfg.defObjectType)
			{
			case MAIN:
				CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE);
				break;
			case LOW:
				CheckDlgButton(hWndDlg,IDC_RADIO_LOW,TRUE);
				break;
			case SSR:
				CheckDlgButton(hWndDlg,IDC_RADIO_SSR,TRUE);
				break;
			case LTP:
				CheckDlgButton(hWndDlg,IDC_RADIO_LTP,TRUE);
				break;
			}
		}
		break; // End of WM_INITDIALOG                                 

	case WM_CLOSE:
		// Closing the Dialog behaves the same as Cancel               
		PostMessage(hWndDlg, WM_COMMAND, IDCANCEL, 0);
		break; // End of WM_CLOSE                                      

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			{
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MAIN))
					CfgDecoder->DecCfg.defObjectType=MAIN;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LOW))
					CfgDecoder->DecCfg.defObjectType=LOW;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_SSR))
					CfgDecoder->DecCfg.defObjectType=SSR;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LTP))
					CfgDecoder->DecCfg.defObjectType=LTP;

				EndDialog(hWndDlg, (DWORD)CfgDecoder);
			}
			break;

        case IDCANCEL:
			// Ignore data values entered into the controls        
			// and dismiss the dialog window returning FALSE
			EndDialog(hWndDlg, (DWORD)FALSE);
			break;

		case IDC_BTN_ABOUT:
				DialogBox((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_ABOUT), (HWND)hWndDlg, (DLGPROC)DialogMsgProcAbout);
			break;
		}
		break; // End of WM_COMMAND
	default: 
		return FALSE;
	}
 
	return TRUE;
}

// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************

BOOL FAR PASCAL FilterUnderstandsFormat(LPSTR filename)
{
WORD len;

	if((len=lstrlen(filename))>4 && 
		(!strcmpi(filename+len-4,".aac") ||
		!strcmpi(filename+len-4,".mp4")))
		return TRUE;
	return FALSE;
}
// *********************************************************************************************

long FAR PASCAL FilterGetFileSize(HANDLE hInput)
{
	if(!hInput)
		return 0;

DWORD	dst_size;
MYINPUT	*mi;

	GLOBALLOCK(mi,hInput,MYINPUT,return 0);
	dst_size=mi->dst_size;
	
	GlobalUnlock(hInput);

	return dst_size;
}
// *********************************************************************************************

DWORD FAR PASCAL FilterOptionsString(HANDLE hInput, LPSTR szString)
{
	if(!hInput)
	{
		lstrcpy(szString,"");
		return 0;
	}

MYINPUT	*mi;

	GLOBALLOCK(mi,hInput,MYINPUT,return 0);

	sprintf(szString,"MPEG%d - %lu bps\n", mi->file_info.version, mi->file_info.bitrate);
	
	if(mi->IsAAC)  // AAC file --------------------------------------------------------------------
	{
		switch(mi->file_info.headertype)
		{
		case RAW:
			lstrcpy(szString,"AAC\nRaw");
			GlobalUnlock(hInput);
			return 0; // call FilterGetOptions()
		case ADIF:
			lstrcat(szString,"ADIF\n");
			break;
		case ADTS:
			lstrcat(szString,"ADTS\n");
			break;
		}
		
		switch(mi->file_info.object_type)
		{
		case MAIN:
			lstrcat(szString,"Main");
			break;
		case LOW:
			lstrcat(szString,"Low Complexity");
			break;
		case SSR:
			lstrcat(szString,"SSR (unsupported)");
			break;
		case LTP:
			lstrcat(szString,"Main LTP");
			break;
		}
	}
	else  // MP4 file -----------------------------------------------------------------------------
		lstrcat(szString,mpeg4AudioNames[mi->type]);
	
	GlobalUnlock(hInput);
	return 1; // don't call FilterGetOptions()
}
// *********************************************************************************************

DWORD FAR PASCAL FilterOptions(HANDLE hInput)
{
/*
	FilterGetOptions() is called if this function and FilterSetOptions() are exported and FilterOptionsString() returns 0
	FilterSetOptions() is called only if this function is exported and and it returns 0
*/
	return 0;
}
// ---------------------------------------------------------------------------------------------

DWORD FAR PASCAL FilterSetOptions(HANDLE hInput, DWORD dwOptions, LONG lSamprate, WORD wChannels, WORD wBPS)
{
	return dwOptions;
}
// *********************************************************************************************

void FAR PASCAL CloseFilterInput(HANDLE hInput)
{
	if(!hInput)
		return;

MYINPUT *mi;

	GLOBALLOCK(mi,hInput,MYINPUT,return);
	
// Raw AAC file ----------------------------------------------------------------------------------
	if(mi->file_info.headertype==RAW)
	{
	CRegistry	reg;

		if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME  "\\FAAD"))
			reg.setRegBool("OpenDialog",FALSE);
		else
			MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
	}

// AAC file --------------------------------------------------------------------------------------
	if(mi->aacFile)
		fclose(mi->aacFile);
	
	FREE(mi->buffer);
	
// MP4 file --------------------------------------------------------------------------------------
	if(mi->mp4File)
		MP4Close(mi->mp4File);
	
	if(mi->hDecoder)
		faacDecClose(mi->hDecoder);
	
// GLOBAL ----------------------------------------------------------------------------------------
//	FREE(mi->src_name);

	GlobalUnlock(hInput);
	GlobalFree(hInput);
}
// *********************************************************************************************

#define ERROR_OFI(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, APP_NAME " plugin", MB_OK|MB_ICONSTOP); \
	if(hInput) \
	{ \
		GlobalUnlock(hInput); \
		CloseFilterInput(hInput); \
	} \
	return 0; \
}
// -----------------------------------------------------------------------------------------------

extern DWORD FAR PASCAL ReadFilterInput(HANDLE hInput, unsigned char far *bufout, long lBytes);
// return handle that will be passed in to close, and write routines
HANDLE FAR PASCAL OpenFilterInput(LPSTR lpstrFilename, long far *lSamprate, WORD far *wBitsPerSample, WORD far *wChannels, HWND hWnd, long far *lChunkSize)
{
HANDLE					hInput;
MYINPUT					*mi;
faacDecConfigurationPtr	config;
DWORD					samplerate;
BYTE					channels,
						BitsPerSample=16;
DWORD					Bitrate4RawAAC=0;
BYTE					Channels4RawAAC=0;

start_point:

	if(!(hInput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE|GMEM_ZEROINIT,sizeof(MYINPUT))))
		ERROR_OFI("Memory allocation error: hInput");
	if(!(mi=(MYINPUT *)GlobalLock(hInput)))
		ERROR_OFI("GlobalLock(hInput)");

	mi->IsAAC=strcmpi(lpstrFilename+lstrlen(lpstrFilename)-4,".aac")==0;

	if(!mi->IsAAC) // MP4 file ---------------------------------------------------------------------
	{
	MP4Duration			length;
	int					track;
	unsigned __int32	buffer_size;
	DWORD				timeScale;
	BYTE				sf, dummy8;

		if(!(mi->mp4File=MP4Read(lpstrFilename, 0)))
		    ERROR_OFI("Error opening file");

		if ((track=GetAACTrack(mi->mp4File))<0)
			ERROR_OFI(0); //"Unable to find correct AAC sound track");

		if(!(mi->hDecoder=faacDecOpen()))
			ERROR_OFI("Error initializing decoder library");

		MP4GetTrackESConfiguration(mi->mp4File, track, (unsigned __int8 **)&mi->buffer, &buffer_size);
	    if(!mi->buffer)
			ERROR_OFI("MP4GetTrackESConfiguration");
		AudioSpecificConfig(mi->buffer, &timeScale, &channels, &sf, &mi->type, &dummy8, &dummy8, &dummy8, &dummy8);
		if(faacDecInit2(mi->hDecoder, mi->buffer, buffer_size, &samplerate, &channels) < 0)
			ERROR_OFI("Error initializing decoder library");
	    FREE(mi->buffer);

		length=MP4GetTrackDuration(mi->mp4File, track);
		mi->len_ms=(DWORD)MP4ConvertFromTrackDuration(mi->mp4File, track, length, MP4_MSECS_TIME_SCALE);
		mi->file_info.bitrate=MP4GetTrackBitRate(mi->mp4File, track);
		mi->file_info.version=MP4GetTrackAudioType(mi->mp4File, track)==MP4_MPEG4_AUDIO_TYPE ? 4 : 2;
		mi->numSamples=MP4GetTrackNumberOfSamples(mi->mp4File, track);
		mi->track=track;
		mi->sampleId=1;
	}
	else // AAC file ------------------------------------------------------------------------------
	{   
	DWORD	read;
	DWORD	*seek_table=0;
	int		seek_table_length=0;
	long	tagsize;
	DWORD	tmp;

		if(!(mi->aacFile=fopen(lpstrFilename,"rb")))
			ERROR_OFI("Error opening file"); 

		// use bufferized stream
		setvbuf(mi->aacFile,NULL,_IOFBF,32767);

		// get size of file
		fseek(mi->aacFile, 0, SEEK_END);
		mi->src_size=ftell(mi->aacFile);
		fseek(mi->aacFile, 0, SEEK_SET);

		if(!(mi->buffer=(BYTE *)malloc(FAAD_STREAMSIZE)))
			ERROR_OFI("Memory allocation error"); 
		memset(mi->buffer,0,FAAD_STREAMSIZE);

		tmp=mi->src_size<FAAD_STREAMSIZE ? mi->src_size : FAAD_STREAMSIZE;
		read=fread(mi->buffer, 1, tmp, mi->aacFile);
		if(read==tmp)
		{
			mi->bytes_read=read;
			mi->bytes_into_buffer=read;
		}
		else
			ERROR_OFI("fread");

		if(tagsize=id3v2_tag(mi->buffer))
		{
			if(tagsize>(long)mi->src_size)
				ERROR_OFI("Corrupt stream!");
			if(tagsize<mi->bytes_into_buffer)
			{
				mi->bytes_into_buffer-=tagsize;
				memcpy(mi->buffer,mi->buffer+tagsize,mi->bytes_into_buffer);
			}
			else
			{
				mi->bytes_read=tagsize;
				mi->bytes_into_buffer=0;
				if(tagsize>mi->bytes_into_buffer)
					fseek(mi->aacFile, tagsize, SEEK_SET);
			}
			if(mi->src_size<mi->bytes_read+FAAD_STREAMSIZE-mi->bytes_into_buffer)
				tmp=mi->src_size-mi->bytes_read;
			else
				tmp=FAAD_STREAMSIZE-mi->bytes_into_buffer;
			read=fread(mi->buffer+mi->bytes_into_buffer, 1, tmp, mi->aacFile);
			if(read==tmp)
			{
				mi->bytes_read+=read;
				mi->bytes_into_buffer+=read;
			}
			else
				ERROR_OFI("fread");

			mi->tagsize=tagsize;
		}

		if(get_AAC_format(lpstrFilename, &mi->file_info, &seek_table, &seek_table_length, 0))
		{
			FREE(seek_table);
			ERROR_OFI("Error retrieving information from input file");
		}
		FREE(seek_table);

		if(!(mi->hDecoder=faacDecOpen()))
			ERROR_OFI("Can't open library");

		if(mi->file_info.headertype==RAW)
			if(!*lSamprate && !*wBitsPerSample && !*wChannels)
			{
			CRegistry	reg;

				if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME  "\\FAAD"))
					reg.setRegBool("OpenDialog",TRUE);
				else
					ERROR_OFI("Can't open registry!");
			/*
			CoolEdit resamples audio if the following code is activated
				*wBitsPerSample=BitsPerSample;
				*wChannels=2;
				*lSamprate=44100;
			*/
				GlobalUnlock(hInput);
				return hInput;
			}
			else
			{
			MY_DEC_CFG	Cfg;

				ReadCfgDec(&Cfg);
				config=faacDecGetCurrentConfiguration(mi->hDecoder);
				config->defObjectType=Cfg.DecCfg.defObjectType;
				config->defSampleRate=Cfg.DecCfg.defSampleRate;//*lSamprate; // doesn't work! Why???
				config->outputFormat=Cfg.DecCfg.outputFormat;
				faacDecSetConfiguration(mi->hDecoder, config);

				if(Bitrate4RawAAC)
					mi->file_info.bitrate=Bitrate4RawAAC*Channels4RawAAC;
				else
				{
				DWORD	Samples,
						BytesConsumed;

					if((mi->bytes_consumed=faacDecInit(mi->hDecoder, mi->buffer, &samplerate, &channels)) < 0)
						ERROR_OFI("Can't init library");
					mi->bytes_into_buffer-=mi->bytes_consumed;
					if(!(Samples=ReadFilterInput(hInput,0,0)/sizeof(short)))
						ERROR_OFI(0);
					BytesConsumed=mi->bytesconsumed;
					ReadFilterInput(hInput,0,0);
					if(BytesConsumed>mi->bytesconsumed)
						mi->bytesconsumed=BytesConsumed;
					Bitrate4RawAAC=(mi->bytesconsumed*8*Cfg.DecCfg.defSampleRate)/Samples;
					if(!Bitrate4RawAAC)
						Bitrate4RawAAC=1000; // try to continue decoding
					Channels4RawAAC=(BYTE)mi->Channels;
					if(!Channels4RawAAC)
						ERROR_OFI("Channels reported by decoder: 0");
					if(Channels4RawAAC!=Cfg.Channels)
					{
					char	buf[256]="";
						sprintf(buf,"Channels reported by decoder: %d",mi->Channels);
						MessageBox(0,buf,0,MB_OK|MB_ICONWARNING);
					}
					GlobalUnlock(hInput);
					CloseFilterInput(hInput);
					goto start_point;
				}
			}

		if((mi->bytes_consumed=faacDecInit(mi->hDecoder, mi->buffer, &samplerate, &channels)) < 0)
			ERROR_OFI("Can't init library");
		mi->bytes_into_buffer-=mi->bytes_consumed;

		if(Channels4RawAAC)
			channels=Channels4RawAAC;

		mi->len_ms=(DWORD)((1000*((float)mi->src_size*8))/mi->file_info.bitrate);
	} // END AAC file -----------------------------------------------------------------------------

	config=faacDecGetCurrentConfiguration(mi->hDecoder);
	switch(config->outputFormat)
	{
	case FAAD_FMT_16BIT:
		BitsPerSample=16;
		break;
	case FAAD_FMT_24BIT:
		BitsPerSample=24;
		break;
	case FAAD_FMT_32BIT:
		BitsPerSample=32;
		break;
	default:
		ERROR_OFI("Invalid Bps");
	}

	if(mi->len_ms)
		mi->dst_size=(DWORD)(mi->len_ms*((float)samplerate/1000)*channels*(BitsPerSample/8));
	else
		mi->dst_size=mi->src_size; // corrupt stream?

	*lSamprate=samplerate;
	*wBitsPerSample=BitsPerSample;
	*wChannels=(WORD)channels;
	*lChunkSize=(BitsPerSample/8)*1024*channels;

	mi->Channels=(WORD)channels;
	mi->Samprate=samplerate;
	mi->BitsPerSample=*wBitsPerSample;
//	mi->src_name=strdup(lpstrFilename);

	GlobalUnlock(hInput);
	return hInput;
}
// *********************************************************************************************

#define ERROR_RFI(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, APP_NAME " plugin", MB_OK|MB_ICONSTOP); \
	if(hInput) \
		GlobalUnlock(hInput); \
	return 0; \
}
// -----------------------------------------------------------------------------------------------

DWORD FAR PASCAL ReadFilterInput(HANDLE hInput, unsigned char far *bufout, long lBytes)
{
	if(!hInput)
		return 0;
	
DWORD				read,
					tmp,
					BytesDecoded=0;
unsigned char		*buffer=0;
faacDecFrameInfo	frameInfo;
char				*sample_buffer=0;
MYINPUT				*mi;

	GLOBALLOCK(mi,hInput,MYINPUT,return 0);

	if(!mi->IsAAC) // MP4 file --------------------------------------------------------------------------
	{   
	unsigned __int32 buffer_size=0;
    int rc;

		do
		{
			buffer=NULL;
			if(mi->sampleId>=mi->numSamples)
				ERROR_RFI(0);

			rc=MP4ReadSample(mi->mp4File, mi->track, mi->sampleId++, (unsigned __int8 **)&buffer, &buffer_size, NULL, NULL, NULL, NULL);
			if(rc==0 || buffer==NULL)
			{
				FREE(buffer);
				ERROR_RFI("MP4ReadSample")
			}

			sample_buffer=(char *)faacDecDecode(mi->hDecoder,&frameInfo,buffer);
			BytesDecoded=frameInfo.samples*sizeof(short);
			memcpy(bufout,sample_buffer,BytesDecoded);
			FREE(buffer);
		}while(!BytesDecoded && !frameInfo.error);
	}
	else // AAC file --------------------------------------------------------------------------
	{   
		buffer=mi->buffer;
		do
		{
			if(mi->bytes_consumed>0)
			{
				if(mi->bytes_into_buffer)
					memcpy(buffer,buffer+mi->bytes_consumed,mi->bytes_into_buffer);

				if(mi->bytes_read<mi->src_size)
				{
					if(mi->bytes_read+mi->bytes_consumed<mi->src_size)
						tmp=mi->bytes_consumed;
					else
						tmp=mi->src_size-mi->bytes_read;
					read=fread(buffer+mi->bytes_into_buffer, 1, tmp, mi->aacFile);
					if(read==tmp)
					{
						mi->bytes_read+=read;
						mi->bytes_into_buffer+=read;
					}	
				}
				else
					if(mi->bytes_into_buffer)
						memset(buffer+mi->bytes_into_buffer, 0, mi->bytes_consumed);

				mi->bytes_consumed=0;
			}

			if(mi->bytes_into_buffer<1)
				if(mi->bytes_read<mi->src_size)
					ERROR_RFI("ReadFilterInput: buffer empty!")
				else
					ERROR_RFI(0)

			sample_buffer=(char *)faacDecDecode(mi->hDecoder,&frameInfo,buffer);
			BytesDecoded=frameInfo.samples*sizeof(short);
			if(bufout)
				memcpy(bufout,sample_buffer,BytesDecoded);
			else // Data needed to decode Raw files
			{
				mi->bytesconsumed=frameInfo.bytesconsumed;
				mi->Channels=frameInfo.channels;
			}
		    mi->bytes_consumed+=frameInfo.bytesconsumed;
			mi->bytes_into_buffer-=mi->bytes_consumed;
		}while(!BytesDecoded && !frameInfo.error);
	} // END AAC file --------------------------------------------------------------------------

	if(frameInfo.error)
		ERROR_RFI((char *)faacDecGetErrorMessage(frameInfo.error));

	GlobalUnlock(hInput);
	return BytesDecoded;
}
