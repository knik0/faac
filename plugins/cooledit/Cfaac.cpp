/*
FAAC - codec plugin for Cooledit
Copyright (C) 2004 Antonio Foranna

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
ntnfrn_email-temp@yahoo.it
*/

#include "Cfaac.h"



// *********************************************************************************************



Cfaac::Cfaac(HANDLE hOut)
{
	if(hOut)
	{
		hOutput=hOut;
		return;
	}

    if(!(hOutput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE|GMEM_ZEROINIT,sizeof(MYOUTPUT))))
	{
		MessageBox(0, "Memory allocation error: hOutput", APP_NAME " plugin", MB_OK|MB_ICONSTOP); \
		return;
	}
}
// -----------------------------------------------------------------------------------------------

Cfaac::~Cfaac()
{
	if(!hOutput)
		return;

MYOUTPUT *mo;

	GLOBALLOCK(mo,hOutput,MYOUTPUT,return);
	
	if(mo->WrittenSamples)
	{
	int	BytesWritten;
		if(mo->bytes_into_buffer>0)
			memset(mo->bufIn+mo->bytes_into_buffer, 0, (mo->samplesInput*(mo->BitsPerSample>>3))-mo->bytes_into_buffer);
		do
		{
			if((BytesWritten=processData(hOutput,mo->bufIn,mo->bytes_into_buffer))<0)
				MessageBox(0, "~Cfaac: processData", APP_NAME " plugin", MB_OK|MB_ICONSTOP);
			mo->bytes_into_buffer=0;
		}while(BytesWritten>0);
	}

	if(mo->aacFile)
	{
		fclose(mo->aacFile);
		mo->aacFile=0;

	MY_ENC_CFG	cfg;
		getFaacCfg(&cfg);
		if(cfg.Tag.On && mo->Filename)
		{
			WriteAacTag(mo->Filename,&cfg.Tag);
			FREE_ARRAY(mo->Filename);
		}
		FreeTag(&cfg.Tag);
	}
	else
	{
		MP4Close(mo->MP4File);
		mo->MP4File=0;
	}
	
	if(mo->hEncoder)
		faacEncClose(mo->hEncoder);
	
	FREE_ARRAY(mo->bitbuf)
	FREE_ARRAY(mo->buf32bit)
	FREE_ARRAY(mo->bufIn)
	
	GlobalUnlock(hOutput);
	GlobalFree(hOutput);
}

// *********************************************************************************************
//									Utilities
// *********************************************************************************************

#define SWAP32(x) (((x & 0xff) << 24) | ((x & 0xff00) << 8) \
	| ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24))
#define SWAP16(x) (((x & 0xff) << 8) | ((x & 0xff00) >> 8))

void Cfaac::To32bit(int32_t *buf, BYTE *bufi, int size, BYTE samplebytes, BYTE bigendian)
{
int i;

	switch(samplebytes)
	{
	case 1:
		// this is endian clean
		for (i = 0; i < size; i++)
			buf[i] = (bufi[i] - 128) * 65536;
		break;
		
	case 2:
#ifdef WORDS_BIGENDIAN
		if (!bigendian)
#else
			if (bigendian)
#endif
			{
				// swap bytes
				for (i = 0; i < size; i++)
				{
					int16_t s = ((int16_t *)bufi)[i];
					
					s = SWAP16(s);
					
					buf[i] = ((u_int32_t)s) << 8;
				}
			}
			else
			{
				// no swap
				for (i = 0; i < size; i++)
				{
					int s = ((int16_t *)bufi)[i];
					
					buf[i] = s << 8;
				}
			}
			break;
			
	case 3:
		if (!bigendian)
		{
			for (i = 0; i < size; i++)
			{
				int s = bufi[3 * i] | (bufi[3 * i + 1] << 8) | (bufi[3 * i + 2] << 16);
				
				// fix sign
				if (s & 0x800000)
					s |= 0xff000000;
				
				buf[i] = s;
			}
		}
		else // big endian input
		{
			for (i = 0; i < size; i++)
			{
				int s = (bufi[3 * i] << 16) | (bufi[3 * i + 1] << 8) | bufi[3 * i + 2];
				
				// fix sign
				if (s & 0x800000)
					s |= 0xff000000;
				
				buf[i] = s;
			}
		}
		break;
		
	case 4:		
#ifdef WORDS_BIGENDIAN
		if (!bigendian)
#else
			if (bigendian)
#endif
			{
				// swap bytes
				for (i = 0; i < size; i++)
				{
					int s = bufi[i];
					
					buf[i] = SWAP32(s);
				}
			}
			else
				memcpy(buf,bufi,size*sizeof(u_int32_t));
		/*
		int exponent, mantissa;
		float *bufo=(float *)buf;
			
			for (i = 0; i < size; i++)
			{
				exponent=bufi[(i<<2)+3]<<1;
				if(bufi[i*4+2] & 0x80)
					exponent|=0x01;
				exponent-=126;
				mantissa=(DWORD)bufi[(i<<2)+2]<<16;
				mantissa|=(DWORD)bufi[(i<<2)+1]<<8;
				mantissa|=bufi[(i<<2)];
				bufo[i]=(float)ldexp(mantissa,exponent);
			}*/
			break;
	}
}
// *********************************************************************************************

int Cfaac::check_image_header(const char *buf)
{
  if (!strncmp(buf, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8))
    return 1; /* PNG */
  else if (!strncmp(buf, "\xFF\xD8\xFF\xE0", 4) &&
	   !strncmp(buf + 6, "JFIF\0", 5))
    return 1; /* JPEG */
  else if (!strncmp(buf, "GIF87a", 6) || !strncmp(buf, "GIF89a", 6))
    return 1; /* GIF */
  else
    return 0;
}
// -----------------------------------------------------------------------------------------------

int Cfaac::ReadCoverArtFile(char *pCoverArtFile, char **artData)
{
FILE *artFile;

	if(!pCoverArtFile || !*pCoverArtFile)
		return 0;

	if(!(artFile=fopen(pCoverArtFile, "rb")))
	{
		MessageBox(NULL,"ReadCoverArtFile: fopen",NULL,MB_OK);
		return 0;
	}

int r;
char *art;
int	artSize=0;

	fseek(artFile, 0, SEEK_END);
	artSize=ftell(artFile);
	fseek(artFile, 0, SEEK_SET);

	if(!(art=(char *)malloc(artSize)))
	{
		fclose(artFile);
		MessageBox(NULL,"ReadCoverArtFile: Memory allocation error!", NULL, MB_OK);
		return 0;
	}

	r=fread(art, 1, artSize, artFile);
	if(r!=artSize)
	{
		free(art);
		fclose(artFile);
		MessageBox(NULL,"ReadCoverArtFile: Error reading cover art file!", NULL, MB_OK);
		return 0;
	}
	else
		if(artSize<12 || !check_image_header(art))
		{
			// the above expression checks the image signature
			free(art);
			fclose(artFile);
			MessageBox(NULL,"ReadCoverArtFile: Unsupported cover image file format!", NULL, MB_OK);
			return 0;
		}

	FREE_ARRAY(*artData);
	*artData=art;
	fclose(artFile);
	return artSize;
}

void Cfaac::WriteMP4Tag(MP4FileHandle MP4File, MP4TAG *Tag)
{
char	*art=NULL;
DWORD	artSize;

char	buf[512], *faac_id_string, *faac_copyright_string;
	sprintf(buf, "FAAC v%s", (faacEncGetVersion(&faac_id_string, &faac_copyright_string)==FAAC_CFG_VERSION) ? faac_id_string : " wrong libfaac version");
	MP4SetMetadataTool(MP4File, buf);

	if(Tag->artist) MP4SetMetadataArtist(MP4File, Tag->artist);
	if(Tag->writer) MP4SetMetadataWriter(MP4File, Tag->writer);
	if(Tag->title) MP4SetMetadataName(MP4File, Tag->title);
	if(Tag->album) MP4SetMetadataAlbum(MP4File, Tag->album);
	if(Tag->trackno>0) MP4SetMetadataTrack(MP4File, Tag->trackno, Tag->ntracks);
	if(Tag->discno>0) MP4SetMetadataDisk(MP4File, Tag->discno, Tag->ndiscs);
	if(Tag->compilation) MP4SetMetadataCompilation(MP4File, Tag->compilation);
	if(Tag->year) MP4SetMetadataYear(MP4File, Tag->year);
	if(Tag->genre) MP4SetMetadataGenre(MP4File, Tag->genre);
	if(Tag->comment) MP4SetMetadataComment(MP4File, Tag->comment);
	artSize=ReadCoverArtFile(Tag->artFileName,&art);
	if(artSize)
	{
		MP4SetMetadataCoverArt(MP4File, (BYTE *)art, artSize);
		free(art);
	}
}

#define ADD_FIELD(id3Tag,NewFrame,ID3FID,ID3FN,data) \
{ \
	ID3_Frame *NewFrame=new ID3_Frame(ID3FID); \
		NewFrame->Field(ID3FN)=data; \
		id3Tag.AttachFrame(NewFrame); \
}

void Cfaac::WriteAacTag(char *Filename, MP4TAG *Tag)
{
char	buf[512], *faac_id_string, *faac_copyright_string;
char	*art=NULL;
DWORD	artSize;
ID3_Tag id3Tag;
ID3_Frame *Frame;

	id3Tag.Link(Filename);
//	Frame=id3Tag.Find(ID3FID_ALBUM);
//	if(Frame!=NULL)
//		myTag.RemoveFrame(Frame);

	sprintf(buf, "FAAC v%s", (faacEncGetVersion(&faac_id_string, &faac_copyright_string)==FAAC_CFG_VERSION) ? faac_id_string : " wrong libfaac version");
	ADD_FIELD(id3Tag,NewFrame,ID3FID_ENCODEDBY,ID3FN_TEXT,buf);

	ADD_FIELD(id3Tag,NewFrame,ID3FID_LEADARTIST,ID3FN_TEXT,Tag->artist);
	ADD_FIELD(id3Tag,NewFrame,ID3FID_COMPOSER,ID3FN_TEXT,Tag->writer);
	ADD_FIELD(id3Tag,NewFrame,ID3FID_TITLE,ID3FN_TEXT,Tag->title);
	ADD_FIELD(id3Tag,NewFrame,ID3FID_ALBUM,ID3FN_TEXT,Tag->album);
	sprintf(buf,"%d",Tag->trackno);
	ADD_FIELD(id3Tag,NewFrame,ID3FID_TRACKNUM,ID3FN_TEXT,buf);
	ADD_FIELD(id3Tag,NewFrame,ID3FID_YEAR,ID3FN_TEXT,Tag->year);
	ADD_FIELD(id3Tag,NewFrame,ID3FID_CONTENTTYPE,ID3FN_TEXT,Tag->genre);
	ADD_FIELD(id3Tag,NewFrame,ID3FID_COMMENT,ID3FN_TEXT,Tag->comment);
	artSize=ReadCoverArtFile(Tag->artFileName,&art);
	if(artSize)
	{
		ADD_FIELD(id3Tag,NewFrame,ID3FID_PICTURE,ID3FN_PICTURETYPE,Tag->artFileName);
		id3Tag.Update();
		free(art);
	}
	else
		id3Tag.Update();
}

// *********************************************************************************************
//									Main functions
// *********************************************************************************************

void Cfaac::DisplayError(char *ProcName, char *str)
{
char buf[100]="";

	if(str && *str)
	{
		if(ProcName && *ProcName)
			sprintf(buf,"%s: ", ProcName);
		strcat(buf,str);
		MessageBox(0, buf, APP_NAME " plugin", MB_OK|MB_ICONSTOP);
	}

MYOUTPUT *mo;
	GLOBALLOCK(mo,hOutput,MYOUTPUT,return);
	mo->bytes_into_buffer=-1;
	GlobalUnlock(hOutput);
	GlobalUnlock(hOutput);
}
// *********************************************************************************************

void Cfaac::FreeTag(MP4TAG *Tag)
{
	FREE_ARRAY(Tag->artist);
	FREE_ARRAY(Tag->title);
	FREE_ARRAY(Tag->album);
	FREE_ARRAY(Tag->year);
	FREE_ARRAY(Tag->genre);
	FREE_ARRAY(Tag->writer);
	FREE_ARRAY(Tag->comment);
	FREE_ARRAY(Tag->artFileName);
}
// -----------------------------------------------------------------------------------------------

void Cfaac::getFaacCfg(MY_ENC_CFG *cfg)
{ 
CRegistry reg;

	if(reg.OpenCreate(HKEY_CURRENT_USER, REGISTRY_PROGRAM_NAME "\\FAAC"))
	{
		cfg->AutoCfg=reg.GetSetBool(REG_AUTO,DEF_AUTO);
		cfg->SaveMP4=reg.GetSetBool(REG_WRITEMP4,DEF_WRITEMP4);
		cfg->EncCfg.mpegVersion=reg.GetSetDword(REG_MPEGVER,DEF_MPEGVER); 
		cfg->EncCfg.aacObjectType=reg.GetSetDword(REG_PROFILE,DEF_PROFILE); 
		cfg->EncCfg.allowMidside=reg.GetSetDword(REG_MIDSIDE,DEF_MIDSIDE); 
		cfg->EncCfg.useTns=reg.GetSetDword(REG_TNS,DEF_TNS); 
		cfg->EncCfg.useLfe=reg.GetSetDword(REG_LFE,DEF_LFE);
		cfg->UseQuality=reg.GetSetBool(REG_USEQUALTY,DEF_USEQUALTY);
		cfg->EncCfg.quantqual=reg.GetSetDword(REG_QUALITY,DEF_QUALITY); 
		cfg->EncCfg.bitRate=reg.GetSetDword(REG_BITRATE,DEF_BITRATE); 
		cfg->EncCfg.bandWidth=reg.GetSetDword(REG_BANDWIDTH,DEF_BANDWIDTH); 
		cfg->EncCfg.outputFormat=reg.GetSetDword(REG_HEADER,DEF_HEADER); 
		cfg->OutDir=NULL;

		cfg->Tag.On=reg.GetSetByte(REG_TAGON,0);
		cfg->Tag.artist=reg.GetSetStr(REG_ARTIST,"");
		cfg->Tag.title=reg.GetSetStr(REG_TITLE,"");
		cfg->Tag.album=reg.GetSetStr(REG_ALBUM,"");
		cfg->Tag.year=reg.GetSetStr(REG_YEAR,"");
		cfg->Tag.genre=reg.GetSetStr(REG_GENRE,"");
		cfg->Tag.writer=reg.GetSetStr(REG_WRITER,"");
		cfg->Tag.comment=reg.GetSetStr(REG_COMMENT,"");
		cfg->Tag.trackno=reg.GetSetDword(REG_TRACK,0);
		cfg->Tag.ntracks=reg.GetSetDword(REG_NTRACKS,0);
		cfg->Tag.discno=reg.GetSetDword(REG_DISK,0);
		cfg->Tag.ndiscs=reg.GetSetDword(REG_NDISKS,0);
		cfg->Tag.compilation=reg.GetSetByte(REG_COMPILATION,0);
		cfg->Tag.artFileName=reg.GetSetStr(REG_ARTFILE,"");
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}
// -----------------------------------------------------------------------------------------------

void Cfaac::setFaacCfg(MY_ENC_CFG *cfg)
{ 
CRegistry reg;

	if(reg.OpenCreate(HKEY_CURRENT_USER, REGISTRY_PROGRAM_NAME "\\FAAC"))
	{
		reg.SetBool(REG_AUTO,cfg->AutoCfg); 
		reg.SetBool(REG_WRITEMP4,cfg->SaveMP4); 
		reg.SetDword(REG_MPEGVER,cfg->EncCfg.mpegVersion); 
		reg.SetDword(REG_PROFILE,cfg->EncCfg.aacObjectType); 
		reg.SetDword(REG_MIDSIDE,cfg->EncCfg.allowMidside); 
		reg.SetDword(REG_TNS,cfg->EncCfg.useTns); 
		reg.SetDword(REG_LFE,cfg->EncCfg.useLfe); 
		reg.SetBool(REG_USEQUALTY,cfg->UseQuality); 
		reg.SetDword(REG_QUALITY,cfg->EncCfg.quantqual); 
		reg.SetDword(REG_BITRATE,cfg->EncCfg.bitRate); 
		reg.SetDword(REG_BANDWIDTH,cfg->EncCfg.bandWidth); 
		reg.SetDword(REG_HEADER,cfg->EncCfg.outputFormat); 

		reg.SetByte(REG_TAGON,cfg->Tag.On);
		reg.SetStr(REG_ARTIST,cfg->Tag.artist);
		reg.SetStr(REG_TITLE,cfg->Tag.title);
		reg.SetStr(REG_ALBUM,cfg->Tag.album);
		reg.SetStr(REG_YEAR,cfg->Tag.year);
		reg.SetStr(REG_GENRE,cfg->Tag.genre);
		reg.SetStr(REG_WRITER,cfg->Tag.writer);
		reg.SetStr(REG_COMMENT,cfg->Tag.comment);
		reg.SetDword(REG_TRACK,cfg->Tag.trackno); 
		reg.SetDword(REG_NTRACKS,cfg->Tag.ntracks); 
		reg.SetDword(REG_DISK,cfg->Tag.discno); 
		reg.SetDword(REG_NDISKS,cfg->Tag.ndiscs); 
		reg.SetByte(REG_COMPILATION,cfg->Tag.compilation); 
		reg.SetStr(REG_ARTFILE,cfg->Tag.artFileName);
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}
// *********************************************************************************************

HANDLE Cfaac::Init(LPSTR lpstrFilename,long lSamprate,WORD wBitsPerSample,WORD wChannels,long FileSize)
{
MYOUTPUT	*mo;
MY_ENC_CFG	cfg;
DWORD		samplesInput,
			maxBytesOutput;

//	if(wBitsPerSample!=8 && wBitsPerSample!=16) // 32 bit audio from cooledit is in unsupported format
//		return 0;
	if(wChannels>48)	// FAAC supports max 48 tracks!
		return NULL;

	GLOBALLOCK(mo,hOutput,MYOUTPUT,return NULL);

	// open the encoder library
	if(!(mo->hEncoder=faacEncOpen(lSamprate, wChannels, &samplesInput, &maxBytesOutput)))
		return ERROR_Init("Can't open library");

	if(!(mo->bitbuf=(unsigned char *)malloc(maxBytesOutput*sizeof(unsigned char))))
		return ERROR_Init("Memory allocation error: output buffer");

	if(!(mo->bufIn=(BYTE *)malloc(samplesInput*sizeof(int32_t))))
		return ERROR_Init("Memory allocation error: input buffer");

	if(!(mo->buf32bit=(int32_t *)malloc(samplesInput*sizeof(int32_t))))
		return ERROR_Init("Memory allocation error: 32 bit buffer");

	getFaacCfg(&cfg);

	if(cfg.SaveMP4)// || cfg.Tag.On)
		if(!strcmpi(lpstrFilename+lstrlen(lpstrFilename)-4,".aac"))
		{
			strcpy(lpstrFilename+lstrlen(lpstrFilename)-4,".mp4");
		FILE *f=fopen(lpstrFilename,"rb");
			if(f)
			{
			char buf[MAX_PATH+20];
				sprintf(buf,"Overwrite \"%s\" ?",lpstrFilename);
				fclose(f);
				if(MessageBox(NULL,buf,"File already exists!",MB_YESNO|MB_ICONQUESTION)==IDNO)
					return ERROR_Init(0);//"User abort");
			}
		}
		else
			if(strcmpi(lpstrFilename+lstrlen(lpstrFilename)-4,".mp4"))
				strcat(lpstrFilename,".mp4");
	mo->WriteMP4=	!strcmpi(lpstrFilename+lstrlen(lpstrFilename)-4,".mp4") ||
					!strcmpi(lpstrFilename+lstrlen(lpstrFilename)-4,".m4a");

faacEncConfigurationPtr CurFormat=faacEncGetCurrentConfiguration(mo->hEncoder);
	CurFormat->inputFormat=FAAC_INPUT_32BIT;
/*	switch(wBitsPerSample)
	{
	case 16:
		CurFormat->inputFormat=FAAC_INPUT_16BIT;
		break;
	case 24:
		CurFormat->inputFormat=FAAC_INPUT_24BIT;
		break;
	case 32:
		CurFormat->inputFormat=FAAC_INPUT_32BIT;
		break;
	default:
		CurFormat->inputFormat=FAAC_INPUT_NULL;
		break;
	}*/
	if(!cfg.AutoCfg)
	{
	faacEncConfigurationPtr myFormat=&cfg.EncCfg;

		if(cfg.UseQuality)
		{
			CurFormat->quantqual=myFormat->quantqual;
			CurFormat->bitRate=0;//myFormat->bitRate;
		}
		else
			if(!CurFormat->bitRate)
				CurFormat->bitRate=myFormat->bitRate;
			else
				CurFormat->bitRate*=1000;

		switch(CurFormat->bandWidth)
		{
		case 0:
			break;
		case 0xffffffff:
			CurFormat->bandWidth=lSamprate/2;
			break;
		default:
			CurFormat->bandWidth=myFormat->bandWidth;
			break;
		}
		CurFormat->mpegVersion=myFormat->mpegVersion;
		CurFormat->outputFormat=myFormat->outputFormat;
		CurFormat->mpegVersion=myFormat->mpegVersion;
		CurFormat->aacObjectType=myFormat->aacObjectType;
		CurFormat->allowMidside=myFormat->allowMidside;
		CurFormat->useTns=myFormat->useTns;
	}
	else
	{
		CurFormat->mpegVersion=DEF_MPEGVER;
		CurFormat->aacObjectType=DEF_PROFILE;
		CurFormat->allowMidside=DEF_MIDSIDE;
		CurFormat->useTns=DEF_TNS;
		CurFormat->useLfe=DEF_LFE;
		CurFormat->quantqual=DEF_QUALITY;
		CurFormat->bitRate=DEF_BITRATE;
		CurFormat->bandWidth=DEF_BANDWIDTH;
		CurFormat->outputFormat=DEF_HEADER;
	}
	if(mo->WriteMP4)
		CurFormat->outputFormat=RAW;
	CurFormat->useLfe=wChannels>=6 ? 1 : 0;
	if(!faacEncSetConfiguration(mo->hEncoder, CurFormat))
		return ERROR_Init("Unsupported parameters!");

//	mo->src_size=lSize;
//	mi->dst_name=strdup(lpstrFilename);
	mo->Samprate=lSamprate;
	mo->BitsPerSample=wBitsPerSample;
	mo->Channels=wChannels;
	mo->samplesInput=samplesInput;
	mo->samplesInputSize=samplesInput*(mo->BitsPerSample>>3);

	mo->maxBytesOutput=maxBytesOutput;

    if(mo->WriteMP4) // Create MP4 file --------------------------------------------------------------------------
	{
    BYTE *ASC=0;
    DWORD ASCLength=0;

        if((mo->MP4File=MP4Create(lpstrFilename, 0, 0, 0))==MP4_INVALID_FILE_HANDLE)
			return ERROR_Init("Can't create file");
        MP4SetTimeScale(mo->MP4File, 90000);
        mo->MP4track=MP4AddAudioTrack(mo->MP4File, lSamprate, MP4_INVALID_DURATION, MP4_MPEG4_AUDIO_TYPE);
        MP4SetAudioProfileLevel(mo->MP4File, 0x0F);
        faacEncGetDecoderSpecificInfo(mo->hEncoder, &ASC, &ASCLength);
        MP4SetTrackESConfiguration(mo->MP4File, mo->MP4track, (unsigned __int8 *)ASC, ASCLength);
		mo->frameSize=samplesInput/wChannels;
		mo->ofs=mo->frameSize;

		if(cfg.Tag.On)
			WriteMP4Tag(mo->MP4File,&cfg.Tag);
	}
	else // Create AAC file -----------------------------------------------------------------------------
	{
		// open the aac output file 
		if(!(mo->aacFile=fopen(lpstrFilename, "wb")))
			return ERROR_Init("Can't create file");

		// use bufferized stream
		setvbuf(mo->aacFile,NULL,_IOFBF,32767);

		mo->Filename=strdup(lpstrFilename);
	}

	showInfo(mo);

	FreeTag(&cfg.Tag);
	GlobalUnlock(hOutput);
    return hOutput;
}
// *********************************************************************************************

int Cfaac::processData(HANDLE hOutput, BYTE *bufIn, DWORD len)
{
	if(!hOutput)
		return -1;

int bytesWritten=0;
int bytesEncoded;
MYOUTPUT far *mo;

	GLOBALLOCK(mo,hOutput,MYOUTPUT,return 0);

int32_t *buf=mo->buf32bit;

	if((int)len<mo->samplesInputSize)
	{
		mo->samplesInput=(len<<3)/mo->BitsPerSample;
		mo->samplesInputSize=mo->samplesInput*(mo->BitsPerSample>>3);
	}
//	if(mo->BitsPerSample==8 || mo->BitsPerSample==32)
		To32bit(buf,bufIn,mo->samplesInput,mo->BitsPerSample>>3,false);

	// call the actual encoding routine
	if((bytesEncoded=faacEncEncode(mo->hEncoder, (int32_t *)buf, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput))<0)
		return ERROR_processData("faacEncEncode()");

	// write bitstream to aac file 
	if(mo->aacFile)
	{
		if(bytesEncoded>0)
		{
			if((bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->aacFile))!=bytesEncoded)
				return ERROR_processData("fwrite");
			mo->WrittenSamples=1; // needed into destructor
		}
	}
	else
	// write bitstream to mp4 file
	{
	MP4Duration dur,
				SamplesLeft;
		if(len>0)
		{
			mo->srcSize+=len;
			dur=mo->frameSize;
		}
		else
		{
			mo->TotalSamples=(mo->srcSize<<3)/(mo->BitsPerSample*mo->Channels);
			SamplesLeft=(mo->TotalSamples-mo->WrittenSamples)+mo->frameSize;
			dur=SamplesLeft>mo->frameSize ? mo->frameSize : SamplesLeft;
		}
		if(bytesEncoded>0)
		{
			if(!(bytesWritten=MP4WriteSample(mo->MP4File, mo->MP4track, (unsigned __int8 *)mo->bitbuf, (DWORD)bytesEncoded, dur, mo->ofs, true) ? bytesEncoded : -1))
				return ERROR_processData("MP4WriteSample");
			mo->ofs=0;
			mo->WrittenSamples+=dur;
		}
	}

	showProgress(mo);

	GlobalUnlock(hOutput);
	return bytesWritten;
}
// -----------------------------------------------------------------------------------------------

int Cfaac::processDataBufferized(HANDLE hOutput, BYTE *bufIn, long lBytes)
{
	if(!hOutput)
		return -1;

int	bytesWritten=0, tot=0;
MYOUTPUT far *mo;

	GLOBALLOCK(mo,hOutput,MYOUTPUT,return 0);

	if(mo->bytes_into_buffer>=0)
		do
		{
			if(mo->bytes_into_buffer+lBytes<mo->samplesInputSize)
			{
				memmove(mo->bufIn+mo->bytes_into_buffer, bufIn, lBytes);
				mo->bytes_into_buffer+=lBytes;
				lBytes=0;
			}
			else
			{
			int	shift=mo->samplesInputSize-mo->bytes_into_buffer;
				memmove(mo->bufIn+mo->bytes_into_buffer, bufIn, shift);
				mo->bytes_into_buffer+=shift;
				bufIn+=shift;
				lBytes-=shift;

				tot+=bytesWritten=processData(hOutput,mo->bufIn,mo->bytes_into_buffer);
				if(bytesWritten<0)
					return ERROR_processData(0);
				mo->bytes_into_buffer=0;
			}
		}while(lBytes);

	GlobalUnlock(hOutput);
	return tot;
}
