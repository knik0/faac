#ifdef WIN32
#include <windows.h>
#endif
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include "aacenc.h"
#include "bitstream.h"	/* bit stream module */
#include "rateconv.h"

#include "enc.h"	/* encoder core */
////////////////////////////////////////////////////////////////////////////////
int write_ADIF_header(faacAACStream *as)
{
	BsBitStream *bitHeader;
        unsigned char headerBuf[20];
	float seconds;
	int i, bits, bytes;
	static int SampleRates[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,0};

	seconds = (float)as->out_sampling_rate/(float)1024;
	seconds = (float)as->cur_frame/seconds;

	as->total_bits += 17 * 8;
	bitHeader = BsOpenWrite(200);

	for (i = 0; ; i++)
	{
		if (SampleRates[i] == as->out_sampling_rate)
			break;
		else if (SampleRates[i] == 0)
		{
			return FERROR;
		}
	}

	// ADIF_Header
	BsPutBit(bitHeader,'A',8);
	BsPutBit(bitHeader,'D',8);
	BsPutBit(bitHeader,'I',8);
	BsPutBit(bitHeader,'F',8);
	BsPutBit(bitHeader,0,1);   // Copyright present
	BsPutBit(bitHeader,0,1);   // Original
	BsPutBit(bitHeader,0,1);   // Home
	BsPutBit(bitHeader,0,1);   // Bitstream type
	BsPutBit(bitHeader,(int)(as->total_bits/seconds),23);  // Bitrate
	BsPutBit(bitHeader, 0, 4);   // num program config elements

	// ADIF_buffer_fulness
	BsPutBit(bitHeader, 0, 20);

	// program_config_element
	BsPutBit(bitHeader,0,4);
	BsPutBit(bitHeader,as->profile,2);
	BsPutBit(bitHeader,i,4);
	BsPutBit(bitHeader,1,4);
	BsPutBit(bitHeader,0,4);
	BsPutBit(bitHeader,0,4);
	BsPutBit(bitHeader,0,2);
	BsPutBit(bitHeader,0,3);
	BsPutBit(bitHeader,0,4);
	BsPutBit(bitHeader,0,1);
	BsPutBit(bitHeader,0,1);
	BsPutBit(bitHeader,0,1);
	// element_list
	BsPutBit(bitHeader,(as->channels == 2),1);
	BsPutBit(bitHeader,0,4);

	ByteAlign(bitHeader, 1);
        // Comment
	BsPutBit(bitHeader,0,8);

	bits = BsBufferNumBit(bitHeader);

	// Copy bitBuf into bitBuffer here
	bytes = (int)((bits+7)/8);
	for (i = 0; i < bytes; i++)
		headerBuf[i] = bitHeader->data[i];
	BsClose(bitHeader);
        // Write header to a file
	fseek(as->out_file, 0, SEEK_SET);
	fwrite(headerBuf, 1, 17, as->out_file);
	return FNO_ERROR;
}
////////////////////////////////////////////////////////////////////////////////
int faac_EncodeInit(faacAACStream *as, char *in_file, char *out_file)
{
	int frameNumSample,delayNumSample;
	int ch, frames, startupNumFrame;
	SF_INFO sf_info;

	if (as->raw_audio) {
		sf_info.format =  SF_FORMAT_RAW;
		sf_info.format |= SF_FORMAT_PCM_BE;
		sf_info.channels = 2;
		sf_info.pcmbitwidth = 16;
		sf_info.samplerate = 44100;
		}

	as->in_file = sf_open_read(in_file, &sf_info);
	if (as->in_file==NULL)
		return -1;

	as->out_file = fopen(out_file, "wb");
	if (as->out_file==NULL)
		return -2;

	frames = (int)(sf_info.samples/1024+0.5);

	as->channels = sf_info.channels;
	as->in_sampling_rate = sf_info.samplerate;
	as->out_sampling_rate = (as->out_sampling_rate) ? (as->out_sampling_rate) : sf_info.samplerate;
	as->cut_off = (as->cut_off) ? (as->cut_off) : ((as->out_sampling_rate)>>1);

	if ((as->inputBuffer = (double**)malloc( as->channels*sizeof(double*)))==NULL)
		return -3;
	for (ch=0; ch < as->channels; ch++)
	{
		if ((as->inputBuffer[ch]=(double*)malloc( 1024*sizeof(double)))==NULL)
			return -4;
	}

        as->bit_rate*=1000;
        if((as->bit_rate % 1000)||(as->bit_rate < 16000)) {
		return -5;
	}
	if (as->channels != 2)
		return -6;
	if ((as->profile != MAIN_PROFILE)&&(as->profile != LOW_PROFILE))
		return -7;

	as->total_bits = 0;
	as->frames = 0;
	as->cur_frame = 0;
	as->is_first_frame = 1;
        as->is_last_frames = 0;

	if (as->in_sampling_rate != as->out_sampling_rate)
		as->rc_needed = 1;
	else
		as->rc_needed = 0;

	EncTfInit(as);

	frameNumSample = 1024;
	delayNumSample = 2*frameNumSample;

	as->samplesToRead = frameNumSample * as->channels;

	as->frame_bits = (int)(as->bit_rate*frameNumSample/as->out_sampling_rate+0.5);
	as->bitBufferSize = (int)(((as->frame_bits * 5) + 7)/8);


	/* num frames to start up encoder due to delay compensation */
	startupNumFrame = (delayNumSample+frameNumSample-1)/frameNumSample;

	/* process audio file frame by frame */
	as->cur_frame = -startupNumFrame;
	as->available_bits = 8184;

	if (as->rc_needed) {
		as->rc_buf = RateConvInit (0,		/* in: debug level */
			(double)as->out_sampling_rate/(double)as->in_sampling_rate, /* in: outputRate / inputRate */
			as->channels,		/* in: number of channels */
			-1,			/* in: num taps */
			-1,			/* in: alpha for Kaiser window */
			-1,			/* in: 6dB cutoff freq / input bandwidth */
			-1,			/* in: 100dB cutoff freq / input bandwidth */
			&as->samplesToRead);        /* out: num input samples / frame */
	} else {
		as->samplesToRead = 1024*as->channels;
	}

	as->savedSize = 0;

       	if (as->header_type == ADIF_HEADER) {
                char tmp[17];
		memset(tmp, 0, 17*sizeof(char));
		fwrite(tmp, 1, 17, as->out_file);
	}
	as->sampleBuffer = (short*) malloc(as->samplesToRead*sizeof(short));
	as->bitBuffer = (unsigned char*) malloc((as->bitBufferSize+100)*sizeof(char));

	return frames;
}
////////////////////////////////////////////////////////////////////////////////
int faac_EncodeFrame(faacAACStream *as)
{
	int i, j, error;
	int usedNumBit, usedBytes;
	int samplesOut, curSample = 0, Samples;
	BsBitStream *bitBuf;
	float *dataOut;
	float *data = NULL;
	int totalBytes = 0;

        if (!as->is_last_frames){
               	Samples = sf_read_short(as->in_file, as->sampleBuffer, as->samplesToRead);
                if (Samples < as->samplesToRead) as->is_last_frames = 1;
                }
            else {
                 Samples = 0;
                 if(as->sampleBuffer){
                         free(as->sampleBuffer);
                         as->sampleBuffer = NULL;
                         }
                 }

	// Is this the last (incomplete) frame
	if ((Samples < as->samplesToRead)&&(Samples > 0)) {
		// Padd with zeros
		memset(as->sampleBuffer + Samples, 0, (as->samplesToRead-Samples)*sizeof(short));
	}

	if (as->rc_needed && (Samples > 0)) {
		samplesOut = as->savedSize + RateConv (
			as->rc_buf,			/* in: buffer (handle) */
			as->sampleBuffer,		/* in: input data[] */
			as->samplesToRead,		/* in: number of input samples */
			&dataOut);

		data = (float*) malloc((samplesOut)*sizeof(float));
		for (i = 0; i < as->savedSize; i++)
			data[i] = as->saved[i];
		for (j = 0; i < samplesOut; i++, j++)
			data[i] = dataOut[j];
	} else if (Samples > 0) {
		samplesOut = 1024*as->channels;
		data = (float*) malloc((samplesOut)*sizeof(float));
		for (i = 0; i < samplesOut; i++)
			data[i] = as->sampleBuffer[i];
	} else
		samplesOut = 1024*as->channels;

	while(samplesOut >= 1024*as->channels)
	{

		// Process Buffer
		if (as->sampleBuffer) {
			if (as->channels == 2)
			{
				if (Samples > 0)
					for (i = 0; i < 1024; i++)
					{
						as->inputBuffer[0][i] = data[curSample+(i*2)];
						as->inputBuffer[1][i] = data[curSample+(i*2)+1];
					}
				else // (Samples == 0) when called by faacEncodeFinish
					for (i = 0; i < 1024; i++)
					{
						as->inputBuffer[0][i] = 0;
						as->inputBuffer[1][i] = 0;
					}
			} else {
				// No mono supported yet (basically only a problem with decoder
				// the encoder in fact supports it).
				return FERROR;
			}
		}

		if (as->is_first_frame) {
			EncTfFrame(as, (BsBitStream*)NULL);

			as->is_first_frame = 0;
			as->cur_frame++;

			as->bitBufferSize = 0;

			samplesOut -= (as->channels*1024);
			curSample  += (as->channels*1024);

			continue;
		}

		bitBuf = BsOpenWrite(as->frame_bits * 10);

		/* compute available number of bits */
		/* frameAvailNumBit contains number of bits in reservoir */
		/* variable bit rate: don't exceed bit reservoir size */
		if (as->available_bits > 8184)
			as->available_bits = 8184;

		/* Add to frameAvailNumBit the number of bits for this frame */
		as->available_bits += as->frame_bits;

		/* Encode frame */
		error = EncTfFrame(as, bitBuf);

		if (error == FERROR)
			return FERROR;

		usedNumBit = BsBufferNumBit(bitBuf);
		as->total_bits += usedNumBit;

		// Copy bitBuf into bitBuffer here
		usedBytes = (int)((usedNumBit+7)/8);
		for (i = 0; i < usedBytes; i++)
			as->bitBuffer[i+totalBytes] = bitBuf->data[i];
		totalBytes += usedBytes;
		BsClose(bitBuf);

		/* Adjust available bit counts */
		as->available_bits -= usedNumBit;    /* Subtract bits used */

		as->cur_frame++;

		samplesOut -= (as->channels*1024);
		curSample  += (as->channels*1024);

	}

	as->bitBufferSize = totalBytes;

	as->savedSize = samplesOut;
	for (i = 0; i < samplesOut; i++)
		as->saved[i] = data[curSample+i];
	if (data) free(data);

       	fwrite(as->bitBuffer, 1, as->bitBufferSize, as->out_file);
        if (Samples < as->samplesToRead) return F_FINISH;
                else	return FNO_ERROR;
}
////////////////////////////////////////////////////////////////////////////////
void faac_EncodeFinish(faacAACStream *as)
{
	faac_EncodeFrame(as);
	faac_EncodeFrame(as);
}
////////////////////////////////////////////////////////////////////////////////
void faac_EncodeFree(faacAACStream *as)
{
	int ch;

	/* free encoder memory */
	EncTfFree();
	if (as->rc_needed)
		RateConvFree (as->rc_buf);

	for(ch=0; ch < as->channels; ch++)
		if(as->inputBuffer[ch]) free(as->inputBuffer[ch]);
	if(as->inputBuffer) free(as->inputBuffer);

        if (as->header_type==ADIF_HEADER)
             write_ADIF_header(as);

        sf_close(as->in_file);
        fclose(as->out_file);

	if (as->bitBuffer)  free(as->bitBuffer);
}
////////////////////////////////////////////////////////////////////////////////
faacVersion *faac_Version(void)
{
	faacVersion *faacv = malloc(sizeof(faacVersion));

	faacv->DLLMajorVersion = 2;
	faacv->DLLMinorVersion = 30;
	faacv->MajorVersion = 0;
	faacv->MinorVersion = 70;
	strcpy(faacv->HomePage, "http://www.slimline.net/aac/");

	return faacv;
}
////////////////////////////////////////////////////////////////////////////////
void faac_InitParams(faacAACStream *as)
{
as->profile = MAIN_PROFILE;
as->header_type = ADTS_HEADER;
as->use_IS = 0;
as->use_MS = 0;
as->use_TNS = 0;
as->use_LTP = 1;
as->use_PNS = 0;
as->cut_off = 0;
as->bit_rate = 128;
as->out_sampling_rate = 0;
as->raw_audio = 0;
}
////////////////////////////////////////////////////////////////////////////////
#ifdef FAAC_DLL

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    return TRUE;
}

#else  // Not dll

/* You can download libsndfile from http://www.zip.com.au/~erikd/libsndfile/ */
#ifdef WIN32
#include <windows.h>
#else
#include <time.h>
#endif

char *get_filename(char *pPath)
{
    char *pT;

    for (pT = pPath; *pPath; pPath++) {
        if ((pPath[0] == '\\' || pPath[0] == ':') && pPath[1] && (pPath[1] != '\\'))
            pT = pPath + 1;
    }

    return pT;
}

void combine_path(char *path, char *dir, char *file)
{
	/* Should be a bit more sophisticated
	   This code assumes that the path exists */

	/* Assume dir is allocated big enough */
	if (dir[strlen(dir)-1] != '\\')
		strcat(dir, "\\");
	strcat(dir, get_filename(file));
	strcpy(path, dir);
}

void usage(void)
{
	printf("Usage:\n");
	printf("faac.exe -options file ...\n");
	printf("Options:\n");
	printf(" -?    Shows this help screen.\n");
	printf(" -pX   AAC profile (X can be LOW, or MAIN (default).\n");
	printf(" -bX   Bitrate in kbps (in steps of 1kbps, min. 16kbps)\n");
	printf(" -pns  Use PNS (Perceptual Noise Substitution).\n");
	printf(" -tns  Use TNS (Temporal Noise Shaping).\n");
	printf(" -is   Use intensity stereo coding.\n");
	printf(" -ms   Use mid/side stereo coding.\n");
	printf(" -nm   Don't use mid/side stereo coding.\n");
	printf("       The default for MS is intelligent switching.\n");
	printf(" -np   Don't use LTP (Long Term Prediction).\n");
	printf(" -oX   Set output directory.\n");
	printf(" -sX   Set output sampling rate.\n");
	printf(" -cX   Set cut-off frequency.\n");
	printf(" -r    Use raw data input file.\n");
	printf(" -hN   No header will be written to the AAC file.\n");
	printf(" -hS   ADTS headers will be written to the AAC file(default).\n");
	printf(" -hI   ADIF header will be written to the AAC file.\n");
	printf(" file  Multiple files can be given as well as wild cards.\n");
	printf("       Can be any of the filetypes supported by libsndfile\n");
	printf("       (http://www.zip.com.au/~erikd/libsndfile/).\n");
	printf("Example:\n");
	printf("       faac.exe -b96 -oc:\\aac\\ *.wav\n");
	return;
}

int parse_arg(int argc, char *argv[],faacAACStream *as, char *InFileNames[100], char *OutFileNames[100])
{
int i, out_dir_set=0, FileCount=0;
char *argp, *fnp, out_dir[255];

faac_InitParams(as);
	if (argc == 1) {
		usage();
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		if ((argv[i][0] != '-')&&(argv[i][0] != '/')) {
			if (strchr("-/", argv[i][0]))
				argp = &argv[i][1];
			else  argp = argv[i];

			if (!strchr(argp, '*') && !strchr(argp, '?'))
			{
				InFileNames[FileCount] = (char*) malloc((strlen(argv[i])+1)*sizeof(char));
				OutFileNames[FileCount] = (char*) malloc((strlen(argv[i])+1)*sizeof(char));
				strcpy(InFileNames[FileCount], argv[i]);
				FileCount++;
			} else {
#ifdef WIN32
				HANDLE hFindFile;
				WIN32_FIND_DATA fd;

				char path[255], *p;

				if (NULL == (p = strrchr(argp, '\\')))
					p = strrchr(argp, '/');
				if (p)
				{
					char ch = *p;

					*p = 0;
					strcat(strcpy(path, argp), "\\");
					*p = ch;
				}
				else
					*path = 0;

				if (INVALID_HANDLE_VALUE != (hFindFile = FindFirstFile(argp, &fd)))
				{
					do
					{
						InFileNames[FileCount] = (char*) malloc((strlen(fd.cFileName)
							+ strlen(path) + 2)*sizeof(char));
						strcat(strcpy(InFileNames[FileCount], path), fd.cFileName);
						FileCount++;
					} while (FindNextFile(hFindFile, &fd));
					FindClose(hFindFile);
				}
#else
				printf("Wildcards not yet supported on systems other than WIN32\n");
#endif
			}
		} else {
			switch(argv[i][1]) {
			case 'p': case 'P':
				if ((argv[i][2] == 'n') || (argv[i][2] == 'N'))
					as->use_PNS = 1;
				else if ((argv[i][2] == 'l') || (argv[i][2] == 'L'))
					as->profile = LOW_PROFILE;
				else
					as->profile = MAIN_PROFILE;
				break;
			case 'n': case 'N':
				if ((argv[i][2] == 'm') || (argv[i][2] == 'M'))
					as->use_MS = -1;
				else if ((argv[i][2] == 'p') || (argv[i][2] == 'P'))
					as->use_LTP = 0;
				break;
                        case 'h': case 'H':
                                if ((argv[i][2] == 'i') || (argv[i][2] == 'I'))
                                        as->header_type = ADIF_HEADER;
                                else if ((argv[i][2] == 's') || (argv[i][2] == 'S'))
                                        as->header_type = ADTS_HEADER;
                                else if ((argv[i][2] == 'n') || (argv[i][2] == 'N'))
                                        as->header_type = NO_HEADER;
				break;
			case 'm': case 'M':
				as->use_MS = 1;
				break;
			case 'i': case 'I':
				as->use_IS = 1;
				break;
			case 'r': case 'R':
				as->raw_audio = 1;
				break;
			case 't': case 'T':
				as->use_TNS = 1;
				break;
			case 'b': case 'B':
				as->bit_rate = atoi(&argv[i][2]);
				break;
			case 's': case 'S':
				as->out_sampling_rate = atoi(&argv[i][2]);
				break;
			case 'c': case 'C':
				as->cut_off = atoi(&argv[i][2]);
				break;
			case 'o': case 'O':
				out_dir_set = 1;
				strcpy(out_dir, &argv[i][2]);
				break;
			case '?':
				usage();
				return 1;
			}
		}
	}
        if (FileCount == 0) {
		return -1;
        	}
            else{
                for (i = 0; i < FileCount; i++){
        		if (out_dir_set)
        			combine_path(OutFileNames[i], out_dir, InFileNames[i]);
        		else
        			strcpy(OutFileNames[i], InFileNames[i]);
        		fnp = strrchr(OutFileNames[i],'.');
        		fnp[0] = '\0';
        		strcat(OutFileNames[i],".aac");
                }
            }
return FileCount;
}

void printVersion(void)
{
faacVersion *faacv;
faacv = faac_Version();
printf("FAAC cl (Freeware AAC Encoder)\n");
printf("FAAC homepage: %s\n", faacv->HomePage);
printf("Encoder engine version: %d.%d\n\n",
	faacv->MajorVersion, faacv->MinorVersion);
if (faacv) free(faacv);
}

void printConf(faacAACStream *as)
{
printf("AAC configuration:\n");
printf("----------------------------------------------\n");
printf("AAC profile: %s.\n", (as->profile==MAIN_PROFILE)?"MAIN":"LOW");
printf("Bitrate: %dkbps.\n", as->bit_rate);
printf("Mid/Side (MS) stereo coding: %s.\n",
	(as->use_MS==1)?"Full":((as->use_MS==0)?"Switching":"Off"));
printf("Intensity stereo (IS) coding: %s.\n", as->use_IS?"On":"Off");
printf("Temporal Noise Shaping: %s.\n", as->use_TNS?"On":"Off");
printf("Long Term Prediction: %s.\n", as->use_LTP?"On":"Off");
printf("Perceptual Noise Substitution: %s.\n", as->use_PNS?"On":"Off");
if (as->out_sampling_rate)
	printf("Output sampling rate: %dHz.\n", as->out_sampling_rate);
if (as->cut_off)
	printf("Cut-off frequency: %dHz.\n", as->cut_off);
if (as->header_type == ADIF_HEADER)
	printf("Header info: ADIF header (seeking disabled)\n");
    else if (as->header_type == ADTS_HEADER)
	printf("Header info: ADTS headers (seeking enabled)\n");
    else
	printf("Header info: no headers (seeking disabled)\n");
printf("----------------------------------------------\n");
}

int main(int argc, char *argv[])
{
	int i, frames, currentFrame, result, FileCount;
	char *InFileNames[100];
	char *OutFileNames[100];

	faacAACStream *as;

        /* timing vars */
	long begin, end;
	int nTotSecs, nSecs;
	int nMins;

        /* create main aacstream object */
        as =(faacAACStream *) malloc(sizeof(faacAACStream));

        /* Print version of FAAC */
        printVersion();

	/* Process command line params */
        if ((FileCount=parse_arg(argc, argv, as, InFileNames, OutFileNames))<0) return 0;

        /* Print configuration */
        printConf(as);

        /* Process input files */
	for (i = 0; i < FileCount; i++) {
		printf("0%\tBusy encoding %s.\r", InFileNames[i]);
#ifdef WIN32
		begin = GetTickCount();
#else
		begin = clock();
#endif
                /* Init encoder core and retrieve number of frames */
                if ((frames=faac_EncodeInit(as, InFileNames[i], OutFileNames[i])) < 0) {
			printf("Error %d while encoding %s.\n",-frames,InFileNames[i]);
			continue;
		}
                currentFrame = 0;
		// Keep encoding frames until the end of the audio file
		do {
			currentFrame++;
			result = faac_EncodeFrame(as);
			if (result == FERROR) {
				printf("Error while encoding %s.\n", InFileNames[i]);
				break;
			}
			printf("%.2f%%\tBusy encoding %s.\r", min(((double)currentFrame/(double)frames)*100,100),InFileNames[i]);

		} while (result != F_FINISH);

                /* finishing last frames and destroying internal data */
		faac_EncodeFinish(as);
                faac_EncodeFree(as);
#ifdef WIN32
		end = GetTickCount();
#else
		end = clock();
#endif
		nTotSecs = (end-begin)/1000;
		nMins = nTotSecs / 60;
		nSecs = nTotSecs - (60*nMins);
		printf("Encoding %s took:\t%d:%.2d\t\n", InFileNames[i], nMins, nSecs);
                if(InFileNames[i]) free(InFileNames[i]);
                if(OutFileNames[i]) free(OutFileNames[i]);
	}
        if (as) free (as);
	return FNO_ERROR;
}

#endif // end of #ifndef FAAC_DLL

