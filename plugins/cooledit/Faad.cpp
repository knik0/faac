/*
FAAD - codec plugin for Cooledit
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
kreel@interfree.it
*/

#include <windows.h>
#include <stdio.h>		// FILE *
#include "filters.h"	// CoolEdit
#include "defines.h"	// my defines
#include <faac.h>
#include <faad.h>
#include <mp4.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <aacinfo.h>	// get_AAC_format()
#ifdef __cplusplus
}
#endif

// *********************************************************************************************

#define MAX_CHANNELS	2
#define	FAAD_STREAMSIZE	(FAAD_MIN_STREAMSIZE*MAX_CHANNELS)

// -----------------------------------------------------------------------------------------------

#define FREE(ptr) \
{ \
	if(ptr) \
		free(ptr); \
	ptr=0; \
}

// *********************************************************************************************

typedef struct input_tag // any special vars associated with input file
{
//AAC
FILE			*aacFile;
DWORD			src_size;		// size of compressed file
DWORD			tagsize;
DWORD			bytes_read;		// from file
DWORD			bytes_consumed;	// by faadDecDecode
long			bytes_into_buffer;
unsigned char	*buffer;

//MP4
MP4FileHandle	mp4File;
MP4SampleId		sampleId,
				numSamples;
int				track;
BYTE			type;

// GLOBAL
faacDecHandle	hDecoder;
faadAACInfo		file_info;
DWORD			len_ms;			// length of file in milliseconds
WORD			wChannels;
DWORD			dwSamprate;
WORD			wBitsPerSample;
// char			*src_name;		// name of compressed file
DWORD			dst_size;		// size of decoded file. It's needed to set the length of progress bar
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
DWORD dst_size;

	if(hInput)  
	{
		MYINPUT *mi;
		mi=(MYINPUT *)GlobalLock(hInput);
		dst_size=mi->dst_size;
		
		GlobalUnlock(hInput);
	}

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

char buf[20];
MYINPUT *mi;

mi=(MYINPUT *)GlobalLock(hInput);

	lstrcpy(szString,"");
	
	if(mi->file_info.version==2)
		lstrcat(szString,"MPEG2 - ");
	else
		lstrcat(szString,"MPEG4 - ");
	
	sprintf(buf,"%lu bps\n",mi->file_info.bitrate);
	lstrcat(szString,buf);
	
	if(mi->IsAAC)  // AAC file --------------------------------------------------------------------
	{
		switch(mi->file_info.headertype)
		{
		case 0:
			lstrcat(szString,"RAW\n");
			break;
		case 1:
			lstrcat(szString,"ADIF\n");
			break;
		case 2:
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

	return 1;
}
// *********************************************************************************************

void FAR PASCAL CloseFilterInput(HANDLE hInput)
{
	if(!hInput)
		return;

MYINPUT far *mi;

	mi=(MYINPUT far *)GlobalLock(hInput);
	
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

// return handle that will be passed in to close, and write routines
HANDLE FAR PASCAL OpenFilterInput(LPSTR lpstrFilename, long far *lSamprate, WORD far *wBitsPerSample, WORD far *wChannels, HWND hWnd, long far *lChunkSize)
{
HANDLE					hInput;
MYINPUT					*mi;
faacDecConfigurationPtr	config;
DWORD					samplerate;
BYTE					channels,
						BitsPerSample=16;

	hInput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE|GMEM_ZEROINIT,sizeof(MYINPUT));
	if(!hInput)
		ERROR_OFI("Memory allocation error: hInput");
	mi=(MYINPUT *)GlobalLock(hInput);
	memset(mi,0,sizeof(MYINPUT));


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
//		mi->file_info.bitrate=(int)MP4GetTrackIntegerProperty(mi->mp4File, track, "mdia.minf.stbl.stsd.mp4a.esds.decConfigDescr.avgBitrate");
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
		tmp=ftell(mi->aacFile);
		fseek(mi->aacFile, 0, SEEK_END);
		mi->src_size=ftell(mi->aacFile);
		fseek(mi->aacFile, tmp, SEEK_SET);

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

		if(!(mi->hDecoder=faacDecOpen()))
			ERROR_OFI("Can't open library");

		if(get_AAC_format(lpstrFilename, &mi->file_info, &seek_table, &seek_table_length, 0))
		{
			FREE(seek_table);
			ERROR_OFI("Error retrieving information form input file");
		}
		FREE(seek_table);

		if(mi->file_info.headertype==0)
		{
			config=faacDecGetCurrentConfiguration(mi->hDecoder);
			config->defObjectType=mi->file_info.object_type;
			config->defSampleRate=mi->file_info.sampling_rate;
			config->outputFormat=FAAD_FMT_16BIT;
			faacDecSetConfiguration(mi->hDecoder, config);
		}

		if((mi->bytes_consumed=faacDecInit(mi->hDecoder, mi->buffer, &samplerate, &channels)) < 0)
			ERROR_OFI("Can't init library");
		mi->bytes_into_buffer-=mi->bytes_consumed;
/*
		if(mi->bytes_consumed>0)
			faacDecInit reports there is an header to skip
		this operation will be done in ReadFilterInput
*/
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
		ERROR_OFI("Invalid format");
	}

	if(mi->len_ms)
		mi->dst_size=(DWORD)(mi->len_ms*((float)samplerate/1000)*channels*(BitsPerSample/8));
	else
		mi->dst_size=mi->src_size; // corrupt stream?

	*lSamprate=samplerate;
	*wBitsPerSample=BitsPerSample;
	*wChannels=(WORD)channels;
	*lChunkSize=(BitsPerSample/8)*1024*channels;

	mi->wChannels=(WORD)channels;
	mi->dwSamprate=samplerate;
	mi->wBitsPerSample=*wBitsPerSample;
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

DWORD FAR PASCAL ReadFilterInput(HANDLE hInput, unsigned char far *bufout, long lBytes)
{
	if(!hInput)
		ERROR_RFI("Memory allocation error: hInput");

DWORD				read,
					tmp,
					shorts_decoded=0;
unsigned char		*buffer=0;
faacDecFrameInfo	frameInfo;
char				*sample_buffer=0;
MYINPUT				*mi;

	mi=(MYINPUT *)GlobalLock(hInput);

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
			shorts_decoded=frameInfo.samples*sizeof(short);
			memcpy(bufout,sample_buffer,shorts_decoded);
			FREE(buffer);
		}while(!shorts_decoded && !frameInfo.error);
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
					return 0;

			sample_buffer=(char *)faacDecDecode(mi->hDecoder,&frameInfo,buffer);
			shorts_decoded=frameInfo.samples*sizeof(short);
			memcpy(bufout,sample_buffer,shorts_decoded);
		    mi->bytes_consumed +=frameInfo.bytesconsumed;
			mi->bytes_into_buffer-=mi->bytes_consumed;
		}while(!shorts_decoded && !frameInfo.error);
	} // END AAC file --------------------------------------------------------------------------

	GlobalUnlock(hInput);

	if(frameInfo.error)
		ERROR_RFI((char *)faacDecGetErrorMessage(frameInfo.error));

	return shorts_decoded;
}
