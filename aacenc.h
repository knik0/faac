
typedef struct RCBufStruct RCBuf;	/* buffer handle */

#define MAIN_PROFILE 0
#define LOW_PROFILE 1

#define FNO_ERROR 0
#define FERROR 1

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

// This structure is for internal use of the encoder only.
typedef struct {
	long total_bits;
	long frames;
	long cur_frame;
	int is_first_frame;
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
	int samplesToRead;
	int savedSize;
	float saved[2048];
	int cut_off;
	int bit_rate;
	char out_dir[255];
	int out_dir_set;
	int raw_audio;
} faacAACStream;

#ifndef FAAC_DLL


//
// The main() function in encoder.c gives an example of how to use these functions
//

// faacAACStream* faacEncodeInit(faacAACConfig *ac, int *samplesToRead, int *bitBufferSize,
//  int *headerSize);
//
// Purpose:
//  Initializes the DLL.
// Parameters:
//  Input:
//   ac: Completely filled faacAACConfig structure
//  Output:
//   samplesToRead: Number of samples that should be read before every call to faacEncodeFrame()
//   bitBufferSize: Size of the buffer in bytes that should be initialized for writing
//   headerSize: Size of the buffer in bytes that should be initialized for writing the header
//               This number of bytes should be skipped in the file before writing any frames.
//               Later, after calling faacAACFree() the headerBuf should be written to this space in the AAC file.
// Return value:
//  faacAACStream structure that should be used in calls to other functions
typedef int (*FAACENCODEINIT) (faacAACStream *as, int *samplesToRead, int *bitBufferSize, int *headerSize);


// int faacEncodeFrame(faacAACStream *as, short *Buffer, int Samples, unsigned char *bitBuffer,
//  int *bitBufSize);
//
// Purpose:
//  Encodes a chunk of samples.
// Parameters:
//  Input:
//   as: faacAACStream returned by faacEncodeInit()
//   Buffer: Sample data from audio file
//   Samples: Number of samples in buffer
//  Output:
//   bitBuffer: Output data that should be written to the AAC file.
//   bitBufSize: Size of bitBuffer in bytes
// Return value:
//  FERROR or FNO_ERROR
typedef int (*FAACENCODEFRAME) (faacAACStream *as, short *Buffer, int Samples, unsigned char *bitBuffer, int *bitBufSize);


// int faacEncodeFinish(faacAACStream *as, unsigned char *bitBuffer, int *bitBufSize);
//
// Purpose:
//  Flushes the last pieces of data.
// Parameters:
//  Input:
//   as: faacAACStream returned by faacEncodeInit()
//  Output:
//   bitBuffer: Output data that should be written to the AAC file.
//   bitBufSize: Size of bitBuffer in bytes
// Return value:
//  FERROR or FNO_ERROR
typedef int (*FAACENCODEFINISH) (faacAACStream *as, unsigned char *bitBuffer, int *bitBufSize);

// int faacEncodeFree(faacAACStream *as, unsigned char *headerBuf);
//
// Purpose:
//  Frees encoder memory and fills in headerBuf.
// Parameters:
//  Input:
//   as: faacAACStream returned by faacEncodeInit()
//  Output:
//   headerBuf: Header data that should be written to the AAC file, size of 
//         the data is headerSize returned by faacEncodeInit().
// Return value:
//  FERROR or FNO_ERROR
typedef int (*FAACENCODEFREE) (faacAACStream *as, unsigned char *headerBuf);


// int faacEncodeVersion(void);
//
// Purpose:
//  Gives version information from the DLL.
// Return value:
//  Filled in faacVersion structure
typedef faacVersion* (*FAACENCODEVERSION) (void);

#define TEXT_FAACENCODEINIT    "faacEncodeInit"
#define TEXT_FAACENCODEFRAME   "faacEncodeFrame"
#define TEXT_FAACENCODEFINISH  "faacEncodeFinish"
#define TEXT_FAACENCODEFREE    "faacEncodeFree"
#define TEXT_FAACENCODEVERSION "faacEncodeVersion"

#else

__declspec(dllexport) faacAACStream *faacEncodeInit(faacAACStream *as, int *samplesToRead, int *bitBufferSize, int *headerSize);
__declspec(dllexport) int faacEncodeFrame(faacAACStream *as, short *Buffer, int Samples, unsigned char *bitBuffer, int *bitBufSize);
__declspec(dllexport) int faacEncodeFree(faacAACStream *as, unsigned char *headerBuf);
__declspec(dllexport) int faacEncodeFinish(faacAACStream *as, unsigned char *bitBuffer, int *bitBufSize);
__declspec(dllexport) faacVersion *faacEncodeVersion(void);

#endif

