#ifdef WIN32
#include <windows.h>
#endif

#include "aacenc.h"
#include "enc.h"
#include "rateconv.h"

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

  for (i = 0; ; i++){
    if (SampleRates[i] == as->out_sampling_rate)
      break;
    else if (SampleRates[i] == 0)
      return FERROR;
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
  if (as->in_file == NULL)
    return -1;

  as->out_file = fopen(out_file, "wb");
  if (as->out_file == NULL)
    return -2;

  frames = (int)(sf_info.samples/1024+0.5);

  as->channels = sf_info.channels;
  as->in_sampling_rate = sf_info.samplerate;
  as->out_sampling_rate = (as->out_sampling_rate) ? (as->out_sampling_rate) : sf_info.samplerate;
  as->cut_off = (as->cut_off) ? (as->cut_off) : ((as->out_sampling_rate)>>1);

  if ((as->inputBuffer = (double**)malloc( as->channels*sizeof(double*))) == NULL)
    return -3;
  for (ch=0; ch < as->channels; ch++){
    if ((as->inputBuffer[ch]=(double*)malloc( 1024*sizeof(double))) == NULL)
      return -4;
  }

  as->bit_rate*=1000;
  if((as->bit_rate % 1000)||(as->bit_rate < 16000))
    return -5;
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
  }
  else
    as->samplesToRead = 1024*as->channels;

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
    if (Samples < as->samplesToRead)
      as->is_last_frames = 1;
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
  }
  else if (Samples > 0) {
    samplesOut = 1024*as->channels;
    data = (float*) malloc((samplesOut)*sizeof(float));
    for (i = 0; i < samplesOut; i++)
      data[i] = as->sampleBuffer[i];
  }
  else
    samplesOut = 1024*as->channels;

  while(samplesOut >= 1024*as->channels) {
    // Process Buffer
    if (as->sampleBuffer) {
      if (as->channels == 2) {
        if (Samples > 0)
          for (i = 0; i < 1024; i++) {
	    as->inputBuffer[0][i] = data[curSample+(i*2)];
  	    as->inputBuffer[1][i] = data[curSample+(i*2)+1];
	  }
        else // (Samples == 0) when called by faacEncodeFinish
          for (i = 0; i < 1024; i++) {
	    as->inputBuffer[0][i] = 0;
            as->inputBuffer[1][i] = 0;
	  }
        }
      else {
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
  if (data)
    free(data);

   fwrite(as->bitBuffer, 1, as->bitBufferSize, as->out_file);
  if (Samples < as->samplesToRead)
    return F_FINISH;
  else
    return FNO_ERROR;
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
    if(as->inputBuffer[ch])
      free(as->inputBuffer[ch]);
  if(as->inputBuffer)
    free(as->inputBuffer);

  if (as->header_type == ADIF_HEADER)
    write_ADIF_header(as);

  sf_close(as->in_file);
  fclose(as->out_file);

  if (as->bitBuffer)
    free(as->bitBuffer);
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
void faac_SetParam(faacAACStream *as, int param, int value)
{
  switch (param){
    case PROFILE:
      as->profile = value;
      break;
    case HEADER_TYPE:
      as->header_type = value;
      break;
    case MS_STEREO:
      as->use_MS = value;
      break;
    case IS_STEREO:
      as->use_IS = value;
      break;
    case BITRATE:
      as->bit_rate = value;
      break;
    case CUT_OFF:
      as->cut_off = value;
      break;
    case OUT_SAMPLING_RATE:
      as->out_sampling_rate = value;
      break;
    case RAW_AUDIO:
      as->raw_audio = value;
      break;
    case TNS:
      as->use_TNS = value;
      break;
    case LTP:
      as->use_LTP = value;
      break;
    case PNS:
      as->use_PNS = value;
      break;
  }
}
////////////////////////////////////////////////////////////////////////////////
