#include <windows.h>
#include <stdio.h>  // FILE *
#include "filters.h" //CoolEdit
#include "faad.h"
#include "faac.h"
#include "aacinfo.h"
#include "..\..\..\faad2\common\mp4v2\mp4.h"


#define MAX_CHANNELS 2
#define QWORD __int32


typedef struct input_tag // any special vars associated with input file
{
//AAC
 FILE	*fFile;
 DWORD	lSize;    
 DWORD	tagsize;
 DWORD	bytes_read;		// from file
 DWORD	bytes_consumed;	// by faadDecDecode
 long	bytes_into_buffer;
 unsigned char	*buffer;

//MP4
 MP4FileHandle	mp4File;
 MP4SampleId	sampleId, numSamples;
 int			track;
 unsigned char  type;

// GENERAL
 faacDecHandle	hDecoder;
 faadAACInfo	file_info;
 QWORD	len_ms;
 WORD	wChannels;
 DWORD	dwSamprate;
 WORD	wBitsPerSample;
 char	szName[256];
 DWORD	full_size;		// size of decoded file needed to set the length of progress bar
 bool	IsAAC;
} MYINPUT;

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
 "Wavetable synthesis",
 "General MIDI",
 "Algorithmic Synthesis and Audio FX",
 "Reserved"
};

int id3v2_tag(unsigned char *buffer)
{
 if(StringComp((const char *)buffer, "ID3", 3) == 0) 
 {
unsigned long tagsize;

// high bit is not used 
  tagsize=(buffer[6] << 21) | (buffer[7] << 14) |
          (buffer[8] <<  7) | (buffer[9] <<  0);
  tagsize += 10;
  return tagsize;
 }
 else 
  return 0;
}

int GetAACTrack(MP4FileHandle infile)
{
    /* find AAC track */
    int i, rc;
	int numTracks = MP4GetNumberOfTracks(infile, NULL);

	for (i = 0; i < numTracks; i++)
    {
        MP4TrackId trackId = MP4FindTrackId(infile, i, NULL);
        const char* trackType = MP4GetTrackType(infile, trackId);

        if (!strcmp(trackType, MP4_AUDIO_TRACK_TYPE))
        {
            unsigned char *buff = NULL;
            unsigned __int32 buff_size = 0;
			DWORD dummy;
            unsigned char ch_dummy;
            MP4GetTrackESConfiguration(infile, trackId, &buff, &buff_size);

            if (buff)
            {
                rc = AudioSpecificConfig(buff, &dummy, &ch_dummy, &ch_dummy, &ch_dummy);
                free(buff);

                if (rc < 0)
                    return -1;
                return trackId;
            }
        }
    }

    /* can't decode this */
    return -1;
}



// *********************************************************************************************



__declspec(dllexport) BOOL FAR PASCAL FilterUnderstandsFormat(LPSTR filename)
{
WORD len;
 if((len=lstrlen(filename))>4 && 
	(!strcmpi(filename+len-4,".aac") ||
	 !strcmpi(filename+len-4,".mp4")))
  return TRUE;
 return FALSE;
}

__declspec(dllexport) long FAR PASCAL FilterGetFileSize(HANDLE hInput)
{
DWORD full_size;

 if(hInput)  
 {
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);
  full_size=mi->full_size;

  GlobalUnlock(hInput);
 }

 return full_size;
}


__declspec(dllexport) DWORD FAR PASCAL FilterOptionsString(HANDLE hInput, LPSTR szString)
{
char buf[20];

 if(hInput)
 {
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);
 
  lstrcpy(szString,"");

  if(mi->file_info.version == 2)
   lstrcat(szString,"MPEG2 - ");
  else
   lstrcat(szString,"MPEG4 - ");
 
  sprintf(buf,"%lu bps\n",mi->file_info.bitrate);
  lstrcat(szString,buf);
 
  if(mi->IsAAC)
  {
	switch(mi->file_info.headertype)
	{
	case 0:
		lstrcat(szString,"RAW\n");
		return 0L;
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
  else
	lstrcat(szString,mpeg4AudioNames[mi->type]);

  GlobalUnlock(hInput);
 }
 return 1;
}

__declspec(dllexport) DWORD FAR PASCAL FilterGetFirstSpecialData(HANDLE hInput, 
	SPECIALDATA * psp)
{
return 0L;
}
    
__declspec(dllexport) DWORD FAR PASCAL FilterGetNextSpecialData(HANDLE hInput, SPECIALDATA * psp)
{	return 0; // only has 1 special data!  Otherwise we would use psp->hSpecialData
			  // as either a counter to know which item to retrieve next, or as a
			  // structure with other state information in it.
}

__declspec(dllexport) void FAR PASCAL CloseFilterInput(HANDLE hInput)
{
 if(hInput)
 {
 MYINPUT far *mi;
  mi=(MYINPUT far *)GlobalLock(hInput);

  if(mi->fFile)
   fclose(mi->fFile);
  
  if(mi->buffer)
   free(mi->buffer);

  if(mi->hDecoder)
   faacDecClose(mi->hDecoder);

  GlobalUnlock(hInput);
  GlobalFree(hInput);
 }
}


#define ERROR_ON_OPEN_MP4(msg) \
{ \
	MessageBox(0, msg, "FAAD interface", MB_OK); \
	if(infile) \
		MP4Close(infile); \
	if(hDecoder) \
		faacDecClose(hDecoder); \
	GlobalUnlock(hInput); \
	return 0; \
}
#define ERROR_ON_OPEN_AAC(msg) \
{ \
	MessageBox(0, msg, "FAAD interface", MB_OK); \
	fclose(infile); \
	if(buffer) \
		free(buffer); \
	if(hDecoder) \
		faacDecClose(hDecoder); \
	GlobalUnlock(hInput); \
	return 0; \
}

// return handle that will be passed in to close, and write routines
__declspec(dllexport) HANDLE FAR PASCAL OpenFilterInput( LPSTR lpstrFilename,
											long far *lSamprate,
											WORD far *wBitsPerSample,
											WORD far *wChannels,
											HWND hWnd,
											long far *lChunkSize)
{
HANDLE hInput;
MYINPUT *mi;
faacDecHandle hDecoder=0;
faacDecConfigurationPtr config;
DWORD  tmp;
DWORD  samplerate;
unsigned char channels;
unsigned char *buffer=0;

 hInput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE|GMEM_ZEROINIT,sizeof(MYINPUT));
 if(!hInput)
  return 0;
 mi=(MYINPUT *)GlobalLock(hInput);
 memset(mi,0,sizeof(MYINPUT));


 mi->IsAAC=strcmpi(lpstrFilename+lstrlen(lpstrFilename)-4,".aac")==0;

 if(!mi->IsAAC) // MP4 file ---------------------------------------------------------------------
 {
// faacDecFrameInfo frameInfo;
 MP4FileHandle infile;
 MP4SampleId numSamples;
 MP4Duration length;
 int fileType=FAAD_FMT_16BIT; // default
 int track;
 unsigned __int32 buffer_size;
 unsigned long timeScale;
 unsigned char sf;

	if(!(infile = MP4Read(lpstrFilename, 0)))
	    ERROR_ON_OPEN_MP4("Error opening file");
	mi->mp4File=infile;

    if ((track = GetAACTrack(infile)) < 0)
		ERROR_ON_OPEN_MP4("Unable to find correct AAC sound track in the MP4 file");

    length = MP4GetTrackDuration(infile, track);
	mi->len_ms=(DWORD) MP4ConvertFromTrackDuration(infile, track, length, MP4_MSECS_TIME_SCALE);

    if(!(hDecoder=faacDecOpen()))
		ERROR_ON_OPEN_MP4("Can't init library");
    buffer = NULL;
    buffer_size = 0;
    MP4GetTrackESConfiguration(infile, track, &buffer, &buffer_size);
    if(buffer)
		AudioSpecificConfig(buffer, &timeScale, &channels, &sf, &mi->type);
    mi->file_info.bitrate=(int)MP4GetTrackIntegerProperty(infile, track, "mdia.minf.stbl.stsd.mp4a.esds.decConfigDescr.avgBitrate");
    numSamples = MP4GetTrackNumberOfSamples(infile, track);

    if(faacDecInit2(hDecoder, buffer, buffer_size, &samplerate, &channels) < 0)
		ERROR_ON_OPEN_MP4("Error initializing decoder library");

    if (buffer) free(buffer);

   mi->numSamples=numSamples;
   mi->track=track;
   mi->sampleId=1;
 }
 else // AAC file ------------------------------------------------------------------------------
 {   
 FILE   *infile;
 DWORD  pos; // into the file. Needed to obtain length of file
 DWORD  read;
 int    *seek_table;
 long tagsize;

  if(!(infile=fopen(lpstrFilename,"rb")))
		ERROR_ON_OPEN_AAC("Error opening file"); 

  mi->fFile=infile;
  pos=ftell(infile);
  fseek(infile, 0, SEEK_END);
  mi->lSize=ftell(infile);
  fseek(infile, pos, SEEK_SET);

  if(!(buffer=(BYTE *)malloc(768*MAX_CHANNELS)))
  {
   MessageBox(0, "Memory allocation error: buffer", "FAAD interface", MB_OK);
   GlobalUnlock(hInput);
   return 0;
  }
  mi->buffer=buffer;
  memset(buffer, 0, 768*MAX_CHANNELS);

  if(mi->lSize<768*MAX_CHANNELS)
   tmp=mi->lSize;
  else
   tmp=768*MAX_CHANNELS;
  read=fread(buffer, 1, tmp, infile);
  if(read==tmp)
  {
   mi->bytes_read=read;
   mi->bytes_into_buffer=read;
  }
  else
	ERROR_ON_OPEN_AAC("fread");

  tagsize=id3v2_tag(buffer);
  if(tagsize)
  {
   memcpy(buffer,buffer+tagsize,768*MAX_CHANNELS - tagsize);

   if(mi->bytes_read+tagsize<mi->lSize)
	tmp=tagsize;
   else
    tmp=mi->lSize-mi->bytes_read;
   read=fread(buffer+mi->bytes_into_buffer, 1, tmp, mi->fFile);
   if(read==tmp)
   {
    mi->bytes_read+=read;
    mi->bytes_into_buffer+=read;
   }
   else
	ERROR_ON_OPEN_AAC("fread");
  }
  mi->tagsize=tagsize;

  if(!(hDecoder=faacDecOpen()))
	ERROR_ON_OPEN_AAC("Can't init library");

  config = faacDecGetCurrentConfiguration(hDecoder);
//  config->defObjectType = MAIN;
  config->defSampleRate = 44100;
  config->outputFormat=FAAD_FMT_16BIT;
  faacDecSetConfiguration(hDecoder, config);

  if((mi->bytes_consumed=faacDecInit(hDecoder, buffer, &samplerate, &channels)) < 0)
   ERROR_ON_OPEN_AAC("Error retrieving information form input file");
  mi->bytes_into_buffer-=mi->bytes_consumed;
// if(mi->bytes_consumed>0) 
// faacDecInit reports there is an header to skip
// this operation will be done in ReadFilterInput


  if(seek_table=(int *)malloc(sizeof(int)*10800))
  {
   get_AAC_format(lpstrFilename, &(mi->file_info), seek_table);
   free(seek_table);
  }
  if(!mi->file_info.version)
   ERROR_ON_OPEN_AAC("Error retrieving information form input file");

  mi->len_ms=(DWORD)((1000*((float)mi->lSize*8))/mi->file_info.bitrate);
 } // END AAC file -----------------------------------------------------------------------------

  if(mi->len_ms)
   mi->full_size=(DWORD)(mi->len_ms*((float)samplerate/1000)*channels*(16/8));
  else
   mi->full_size=mi->lSize; // corrupted stream?

  mi->hDecoder=hDecoder;
  *lSamprate=samplerate;
  *wBitsPerSample=16;
  *wChannels=(WORD)channels;
  *lChunkSize=sizeof(short)*1024*channels;

  mi->wChannels=(WORD)channels;
  mi->dwSamprate=samplerate;
  mi->wBitsPerSample=*wBitsPerSample;
  strcpy(mi->szName,lpstrFilename);

  GlobalUnlock(hInput);

 return hInput;
}

#define ERROR_ReadFilterInput(msg) \
	{ \
		if(msg) \
			MessageBox(0, msg, "FAAD interface", MB_OK); \
		GlobalUnlock(hInput); \
		return 0; \
	} \

__declspec(dllexport) DWORD FAR PASCAL ReadFilterInput(HANDLE hInput, unsigned char far *bufout, long lBytes)
{
DWORD	read,
		tmp,
		shorts_decoded=0;
unsigned char *buffer=0;
faacDecFrameInfo frameInfo;
char *sample_buffer=0;
MYINPUT *mi;

	if(!hInput)
		return 0;
	mi=(MYINPUT *)GlobalLock(hInput);

	if(!mi->IsAAC) // MP4 file --------------------------------------------------------------------------
	{   
	unsigned __int32 buffer_size=0;
    int rc;

		do
		{
			buffer=NULL;
			if(mi->sampleId>=mi->numSamples)
				ERROR_ReadFilterInput(0);

			rc=MP4ReadSample(mi->mp4File, mi->track, mi->sampleId++, &buffer, &buffer_size, NULL, NULL, NULL, NULL);
			if(rc==0 || buffer==NULL)
			{
				if(buffer) free(buffer);
				ERROR_ReadFilterInput("MP4ReadSample")
			}

			sample_buffer=(char *)faacDecDecode(mi->hDecoder,&frameInfo,buffer);
			shorts_decoded=frameInfo.samples*sizeof(short);
			memcpy(bufout,sample_buffer,shorts_decoded);
			if (buffer) free(buffer);
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

				if(mi->bytes_read<mi->lSize)
				{
					if(mi->bytes_read+mi->bytes_consumed<mi->lSize)
						tmp=mi->bytes_consumed;
					else
						tmp=mi->lSize-mi->bytes_read;
					read=fread(buffer+mi->bytes_into_buffer, 1, tmp, mi->fFile);
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
				if(mi->bytes_read<mi->lSize)
					ERROR_ReadFilterInput("ReadFilterInput: buffer empty!")
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
		ERROR_ReadFilterInput(faacDecGetErrorMessage(frameInfo.error));

	return shorts_decoded;
}
