
/* Encoder DLL interface module.*/

#ifdef FAAC_DLL
#include <windows.h>
#endif
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#include "aacenc.h"
#include "bitstream.h"	/* bit stream module */

#include "enc.h"	/* encoder cores */

#define BITHEADERBUFSIZE (65536)

/* ---------- functions ---------- */


faacAACStream *faacEncodeInit(faacAACConfig *ac, int *samplesToRead, int *bitBufferSize, int *headerSize)
{
	int frameNumSample,delayNumSample;
	int ch;
	faacAACStream *as;

	int startupNumFrame;

	as = malloc( sizeof(faacAACStream));
	if ((as->inputBuffer = (double**)malloc( ac->channels*sizeof(double*)))==NULL)
		return NULL;
	for (ch=0; ch < ac->channels; ch++)
	{
		if ((as->inputBuffer[ch]=(double*)malloc( 1024*sizeof(double)))==NULL)
			return NULL;
	}

//	if (ac->sampling_rate != 44100)
//		return NULL;
	if((ac->bit_rate % 1000)||(ac->bit_rate < 16000)) {
		return NULL;
	}
	if (ac->channels != 2)
		return NULL;
	if ((ac->profile != MAIN_PROFILE)&&(ac->profile != LOW_PROFILE))
		return NULL;

	as->total_bits = 0;
	as->frames = 0;
	as->cur_frame = 0;
	as->channels = ac->channels;
	as->sampling_rate = ac->sampling_rate;
	as->write_header = ac->write_header;
	as->use_MS = ac->use_MS;
	as->use_IS = ac->use_IS;
	as->use_TNS = ac->use_TNS;
	as->use_LTP = ac->use_LTP;
	as->use_PNS = ac->use_PNS;
	as->profile = ac->profile;
	as->is_first_frame = 1;

	if (as->write_header) {
		*headerSize = 17;
	} else {
		*headerSize = 0;
	}

	EncTfInit(ac, 0);

	frameNumSample = 1024;
	delayNumSample = 2*frameNumSample;

	*samplesToRead = frameNumSample * ac->channels;

	as->frame_bits = (int)(ac->bit_rate*frameNumSample/ac->sampling_rate+0.5);
	*bitBufferSize = (int)(((as->frame_bits * 2) + 7)/8);


	/* num frames to start up encoder due to delay compensation */
	startupNumFrame = (delayNumSample+frameNumSample-1)/frameNumSample;

	/* process audio file frame by frame */
	as->cur_frame = -startupNumFrame;
	as->available_bits = 8184;

	return as;
}

int faacEncodeFrame(faacAACStream *as, short *Buffer, int Samples, unsigned char *bitBuffer, int *bitBufSize)
{
	int i, error;
	int usedNumBit, usedBytes;
	BsBitStream *bitBuf;

	// Is this the last (incomplete) frame
	if ((Samples < 1024*as->channels)&&(Samples > 0)) {
		// Padd with zeros
		memset(Buffer + Samples, 0, (2048-Samples)*sizeof(short));
	}

	// Process Buffer
	if (Buffer) {
		if (as->channels == 2)
		{
			if (Samples > 0)
				for (i = 0; i < 1024; i++)
				{
					as->inputBuffer[0][i] = *Buffer++;
					as->inputBuffer[1][i] = *Buffer++;
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

		*bitBufSize = 0;

		return FNO_ERROR;
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
	*bitBufSize = usedBytes;
	for (i = 0; i < usedBytes; i++)
		bitBuffer[i] = bitBuf->data[i];
	BsClose(bitBuf);

	/* Adjust available bit counts */
	as->available_bits -= usedNumBit;    /* Subtract bits used */

	as->cur_frame++;

	return FNO_ERROR;
}

int faacEncodeFinish(faacAACStream *as, unsigned char *bitBuffer, int *bitBufSize)
{
	unsigned char tmpBitBuffer[2048];
	int tmpBitBufSize, i;
	faacEncodeFrame(as, NULL, 0, tmpBitBuffer, &tmpBitBufSize);
	for (i = 0; i < tmpBitBufSize; i++)
		bitBuffer[i] = tmpBitBuffer[i];
	faacEncodeFrame(as, NULL, 0, tmpBitBuffer, bitBufSize);
	for (i = 0; i < *bitBufSize; i++)
		bitBuffer[i+tmpBitBufSize] = tmpBitBuffer[i];
	*bitBufSize += tmpBitBufSize;

	return FNO_ERROR;
}

int faacEncodeFree(faacAACStream *as, unsigned char *headerBuf)
{
	BsBitStream *bitHeader;
	float seconds;
	int bits, bytes, ch;

	seconds = (float)as->sampling_rate/(float)1024;
	seconds = (float)as->cur_frame/seconds;

	/* free encoder memory */
	EncTfFree();

	if (as->write_header)
	{
		int i;
		static int SampleRates[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,0};

		as->total_bits += 17 * 8;

		bitHeader = BsOpenWrite(BITHEADERBUFSIZE);

		for (i = 0; ; i++)
		{
			if (SampleRates[i] == as->sampling_rate)
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

		ByteAlign(bitHeader);
		// Comment
		BsPutBit(bitHeader,0,8);

		bits = BsBufferNumBit(bitHeader);

		// Copy bitBuf into bitBuffer here
		bytes = (int)((bits+7)/8);
		for (i = 0; i < bytes; i++)
			headerBuf[i] = bitHeader->data[i];
		BsClose(bitHeader);
	}

	for (ch=0; ch < as->channels; ch++)
		if(as->inputBuffer[ch]) free(as->inputBuffer[ch]);
	if(as->inputBuffer) free(as->inputBuffer);
	if (as) free(as);

	return FNO_ERROR;
}

faacVersion *faacEncodeVersion(void)
{
	faacVersion *faacv = malloc(sizeof(faacVersion));

	faacv->DLLMajorVersion = 2;
	faacv->DLLMinorVersion = 15;
	faacv->MajorVersion = 0;
	faacv->MinorVersion = 55;
	strcpy(faacv->HomePage, "http://www.slimline.net/aac/");

	return faacv;
}

#ifdef FAAC_DLL

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  ul_reason_for_call, 
                      LPVOID lpReserved)
{
    return TRUE;
}

#else

/* You can download libsndfile from http://www.zip.com.au/~erikd/libsndfile/ */
#ifdef WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include <sndfile.h>

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
	printf(" -h    Shows this help screen.\n");
	printf(" -pX   AAC profile (X can be LOW, or MAIN (default).\n");
	printf(" -bX   Bitrate in kbps (in steps of 1kbps, min. 16kbps)\n");
	printf(" -pns  Use PNS (Perceptual Noise Substitution).\n");
	printf(" -is   Use intensity stereo coding.\n");
	printf(" -ms   Use mid/side stereo coding.\n");
	printf(" -nm   Don't use mid/side stereo coding.\n");
	printf("       The default for MS is intelligent switching.\n");
	printf(" -nt   Don't use TNS (Temporal Noise Shaping).\n");
	printf(" -np   Don't use LTP (Long Term Prediction).\n");
	printf(" -nh   No header will be written to the AAC file.\n");
	printf(" -oX   Set output directory.\n");
	printf(" -r    Use raw data input file.\n");
	printf(" file  Multiple files can be given as well as wild cards.\n");
	printf("       Can be any of the filetypes supported by libsndfile\n");
	printf("       (http://www.zip.com.au/~erikd/libsndfile/).\n");
	printf("Example:\n");
	printf("       faac.exe -b96 -oc:\\aac\\ *.wav\n");
	return;
}


int main(int argc, char *argv[])
{
	int readNumSample;

	short sampleBuffer[2048];
	unsigned char *bitBuffer = NULL;
	FILE *aacfile;
	SNDFILE *sndfile;
	SF_INFO sf_info;
	int noSamples;
	int error;
	int bitBufSize;
	int curBitBufSize;
	int headerSize;
	int i, frames, cfr;
	int profile = MAIN_PROFILE;
	int no_header = 0;
	int use_IS = 0, use_MS = 0, use_TNS = 1, use_LTP = 1, use_PNS = 0;
	int bit_rate = 128;
	char out_dir[255];
	int out_dir_set = 0;
	int raw_audio = 0;
	char *argp;
	char *FileNames[200];
	int FileCount = 0;

	faacAACStream *as;
	faacAACConfig ac;
	faacVersion *faacv;

	long begin, end;

	faacv = NULL;
	faacv = faacEncodeVersion();
	printf("FAAC cl (Freeware AAC Encoder)\n");
	printf("FAAC homepage: %s\n", faacv->HomePage);
	printf("Encoder engine version: %d.%d\n\n",
		faacv->MajorVersion, faacv->MinorVersion);
	if (faacv) free(faacv);

	/* Process the command line */
	if (argc == 1) {
		usage();
		return 1;
	}

	for (i = 1; i < argc; i++)
	{
		if ((argv[i][0] != '-')&&(argv[i][0] != '/')) {
			if (strchr("-/", argv[i][0]))
				argp = &argv[i][1];
			else  argp = argv[i];

			if (!strchr(argp, '*') && !strchr(argp, '?'))
			{
				FileNames[FileCount] = malloc((strlen(argv[i])+1)*sizeof(char));
				strcpy(FileNames[FileCount], argv[i]);
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
						FileNames[FileCount] = malloc((strlen(fd.cFileName)
							+ strlen(path) + 2)*sizeof(char));
						strcat(strcpy(FileNames[FileCount], path), fd.cFileName);
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
			case 'p':
			case 'P':
				if (argv[i][2] == 'n' || 'N')
					use_PNS = 1;
				else if (argv[i][2] == 'l' || 'L')
					profile = LOW_PROFILE;
				else
					profile = MAIN_PROFILE;
				break;
			case 'n':
			case 'N':
				if (argv[i][2] == 'm' || 'M')
					use_MS = -1;
				else if (argv[i][2] == 't' || 'T')
					use_TNS = 0;
				else if (argv[i][2] == 'p' || 'P')
					use_LTP = 0;
				else
					no_header = 1;
				break;
			case 'm':
			case 'M':
				use_MS = 1;
				break;
			case 'i':
			case 'I':
				use_IS = 1;
				break;
			case 'r':
			case 'R':
				raw_audio = 1;
				break;
			case 'b':
			case 'B':
				bit_rate = atoi(&argv[i][2]);
				break;
			case 'o':
			case 'O':
				out_dir_set = 1;
				strcpy(out_dir, &argv[i][2]);
				break;
			case 'h':
			case 'H':
				usage();
				return 1;
			}
		}
	}

	if (FileCount == 0) {
		return 1;
	}

	for (i = 0; i < FileCount; i++) {
		char aac_fn[255];
		char *fnp;

		printf("0%\tBusy encoding %s.\r", FileNames[i]);

#ifdef WIN32
		begin = GetTickCount();
#else
		begin = clock();
#endif

		if (raw_audio) {
			sf_info.format =  SF_FORMAT_RAW;
			sf_info.format |= SF_FORMAT_PCM_BE;
			sf_info.channels = 2;
			sf_info.pcmbitwidth = 16;
			sf_info.samplerate = 44100;
		}

		sndfile = sf_open_read(FileNames[i], &sf_info);
		if (sndfile==NULL) {
			printf("Error while encoding %s.\n", FileNames[i]);
			continue;
		}

		frames = (int)(sf_info.samples/1024+0.5);

		if (out_dir_set)
			combine_path(aac_fn, out_dir, FileNames[i]);
		else
			strcpy(aac_fn, FileNames[i]);
		fnp = strrchr(aac_fn,'.');
		fnp[0] = '\0';
		strcat(aac_fn,".aac");

		aacfile = fopen(aac_fn, "wb");
		if (aacfile==NULL) {
			printf("Error while encoding %s.\n", FileNames[i]);
			continue;
		}

		ac.channels = sf_info.channels;
		ac.sampling_rate = sf_info.samplerate;
		ac.bit_rate = bit_rate * 1000;
		ac.profile = profile;
		ac.use_MS = use_MS;
		ac.use_IS = use_IS;
		ac.use_TNS = use_TNS;
		ac.use_LTP = use_LTP;
		ac.use_PNS = use_PNS;
		ac.write_header = !no_header;

		as = faacEncodeInit(&ac, &readNumSample, &bitBufSize, &headerSize);
		if (as == NULL) {
			printf("Error while encoding %s.\n", FileNames[i]);
			continue;
		}

		bitBuffer = malloc((bitBufSize+100)*sizeof(char));

		if (headerSize > 0) {
			memset(bitBuffer, 0, headerSize*sizeof(char));
			// Skip headerSize bytes
			// They should be written after calling faacEncodeFree
			fwrite(bitBuffer, 1, headerSize, aacfile);
		}

		cfr = 0;

		// Keep encoding frames until the end of the audio file
		do {
			cfr++;

			noSamples = sf_read_short(sndfile, sampleBuffer, readNumSample);

			error = faacEncodeFrame(as, sampleBuffer, noSamples, bitBuffer, &curBitBufSize);
			if (error == FERROR) {
				printf("Error while encoding %s.\n", FileNames[i]);
				break;
			}

			fwrite(bitBuffer, 1, curBitBufSize, aacfile);
			
			printf("%.2f%%\tBusy encoding %s.\r", min(((double)cfr/(double)frames)*100,100),FileNames[i]);

		} while (noSamples == readNumSample);

		if (error == FERROR) {
			continue;
		}

		error = faacEncodeFinish(as, bitBuffer, &curBitBufSize);
		if (error == FERROR) {
			printf("Error while encoding %s.\n", FileNames[i]);
			continue;
		}

		fwrite(bitBuffer, 1, curBitBufSize, aacfile);

		sf_close(sndfile);

		error = faacEncodeFree(as, bitBuffer);
		if (error == FERROR) {
			printf("Error while encoding %s.\n", FileNames[i]);
			continue;
		}

		// Write the header to the beginning of the file now
		if (headerSize > 0) {
			fseek(aacfile, 0, SEEK_SET);
			fwrite(bitBuffer, 1, headerSize, aacfile);
		}

		fclose(aacfile);
		if (bitBuffer) { free(bitBuffer); bitBuffer = NULL; }

#ifdef WIN32
		end = GetTickCount();
#else
		end = clock();
#endif
		printf("Encoding %s took: %d sec.\n", FileNames[i], (end-begin)/1000);
	}

	return FNO_ERROR;
}


#endif

