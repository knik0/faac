#include <stdio.h>
#include <sndfile.h>

typedef struct RCBufStruct RCBuf;	/* buffer handle */

#define MAIN_PROFILE 0
#define LOW_PROFILE 1

#define FNO_ERROR 0
#define FERROR 1
#define F_FINISH 2

#define NO_HEADER	0
#define ADIF_HEADER     1
#define ADTS_HEADER     2

typedef struct {
	int DLLMajorVersion; // These 2 values should always be checked, because the DLL
	int DLLMinorVersion; // interface can change from version to version.
	int MajorVersion;
	int MinorVersion;
	char HomePage[255];
} faacVersion;

// This structure is for AAC stream object
typedef struct {
	long total_bits;
	long frames;
	long cur_frame;
	int is_first_frame;
	int is_last_frames;
	int channels;
	int out_sampling_rate;
	int in_sampling_rate;
	int frame_bits;
	int available_bits;
        int header_type;
	int use_MS;
	int use_IS;
	int use_TNS;
	int use_LTP;
	int use_PNS;
	int profile;
	double **inputBuffer;
	RCBuf *rc_buf;
	int rc_needed;
	int savedSize;
	float saved[2048];
	int cut_off;
	int bit_rate;
	int raw_audio;
        SNDFILE *in_file;
        FILE *out_file;
        unsigned char *bitBuffer;
        int bitBufferSize;
        short *sampleBuffer;
	int samplesToRead;
} faacAACStream;

#ifndef FAAC_DLL

int faac_EncodeInit(faacAACStream *as, char *in_file, char *out_file);
int faac_EncodeFrame(faacAACStream *as);
void faac_EncodeFree(faacAACStream *as);
void faac_EncodeFinish(faacAACStream *as);
faacVersion *faac_Version(void);
void faac_InitParams(faacAACStream *as);

#else

__declspec(dllexport) int faac_EncodeInit(faacAACStream *as, char *in_file, char *out_file);
__declspec(dllexport) int faac_EncodeFrame(faacAACStream *as);
__declspec(dllexport) void faac_EncodeFree(faacAACStream *as);
__declspec(dllexport) void faac_EncodeFinish(faacAACStream *as);
__declspec(dllexport) faacVersion *faac_Version(void);
__declspec(dllexport) void faac_InitParams(faacAACStream *as);

#endif

