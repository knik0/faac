#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include "aacenc.h"
#include "bitstream.h"

#ifdef FAAC_DLL

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
  return TRUE;
}

#else  // Not dll

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

  for (i = 1; i < argc; i++) {
    if ((argv[i][0] != '-')&&(argv[i][0] != '/')) {
      if (strchr("-/", argv[i][0]))
      	argp = &argv[i][1];
      else
        argp = argv[i];

      if (!strchr(argp, '*') && !strchr(argp, '?')) {
	InFileNames[FileCount] = (char*) malloc((strlen(argv[i])+1)*sizeof(char));
	OutFileNames[FileCount] = (char*) malloc((strlen(argv[i])+1)*sizeof(char));
	strcpy(InFileNames[FileCount], argv[i]);
	FileCount++;
      }
      else {
#ifdef _WIN32
	HANDLE hFindFile;
	WIN32_FIND_DATA fd;

        char path[255], *p;

        if (NULL == (p = strrchr(argp, '\\')))
	  p = strrchr(argp, '/');
        if (p) {
	  char ch = *p;

	  *p = 0;
	  strcat(strcpy(path, argp), "\\");
	  *p = ch;
        }
	else
	  *path = 0;

	  if (INVALID_HANDLE_VALUE != (hFindFile = FindFirstFile(argp, &fd))) {
	     do {
	       InFileNames[FileCount] = (char*) malloc((strlen(fd.cFileName) + strlen(path) + 2)*sizeof(char));
	       OutFileNames[FileCount] = (char*) malloc((strlen(fd.cFileName) + strlen(path) + 2)*sizeof(char));
               strcat(strcpy(InFileNames[FileCount], path), fd.cFileName);
	       FileCount++;
             }
             while (FindNextFile(hFindFile, &fd));
	  FindClose(hFindFile);
        }
#else
	printf("Wildcards not yet supported on systems other than WIN32\n");
#endif
      }
    }
    else {
      switch(argv[i][1]) {
      	case 'p': case 'P':
	  if ((argv[i][2] == 'n') || (argv[i][2] == 'N'))
	    faac_SetParam(as,PNS,USE_PNS);
          else if ((argv[i][2] == 'l') || (argv[i][2] == 'L'))
            faac_SetParam(as,PROFILE,LOW_PROFILE);
	  else if ((argv[i][2] == 'm') || (argv[i][2] == 'M'))
	    faac_SetParam(as,PROFILE,MAIN_PROFILE);
          else {
            printf("Unknown option: %s\n", argv[i]);
            return 0;
          }
          break;
        case 'n': case 'N':
	  if ((argv[i][2] == 'm') || (argv[i][2] == 'M'))
	    faac_SetParam(as,MS_STEREO,NO_MS);
          else if ((argv[i][2] == 'p') || (argv[i][2] == 'P'))
	    faac_SetParam(as,LTP,NO_LTP);
          else {
            printf("Unknown option: %s\n", argv[i]);
            return 0;
          }
	  break;
        case 'h': case 'H':
          if ((argv[i][2] == 'i') || (argv[i][2] == 'I'))
	    faac_SetParam(as,HEADER_TYPE,ADIF_HEADER);
          else if ((argv[i][2] == 's') || (argv[i][2] == 'S'))
	    faac_SetParam(as,HEADER_TYPE,ADTS_HEADER);
          else if ((argv[i][2] == 'n') || (argv[i][2] == 'N'))
	    faac_SetParam(as,HEADER_TYPE,NO_HEADER);
          else {
            printf("Unknown option: %s\n", argv[i]);
            return 0;
          }
	  break;
        case 'm': case 'M':
	  faac_SetParam(as,MS_STEREO,FORCE_MS);
	  break;
	case 'i': case 'I':
	  faac_SetParam(as,IS_STEREO,USE_IS);
	  break;
	case 'r': case 'R':
	  faac_SetParam(as,RAW_AUDIO,USE_RAW_AUDIO);
	  break;
	case 't': case 'T':
	  faac_SetParam(as,TNS,USE_TNS);
	  break;
	case 'b': case 'B':
	  faac_SetParam(as,BITRATE,atoi(&argv[i][2]));
	  break;
	case 's': case 'S':
	  faac_SetParam(as,OUT_SAMPLING_RATE,atoi(&argv[i][2]));
	  break;
        case 'c': case 'C':
	  faac_SetParam(as,CUT_OFF,atoi(&argv[i][2]));
	  break;
        case 'o': case 'O':
	  out_dir_set = 1;
	  strcpy(out_dir, &argv[i][2]);
	  break;
	case '?':
	  usage();
	  return 0;
        default:
          printf("Unknown option: %s\n", argv[i]);
          return 0;
      }
    }   // else  of:  if ((argv[i][0] != '-')&&(argv[i][0] != '/')) {
  } //   for (i = 1; i < argc; i++) {

  if (FileCount == 0)
    return -1;
  else {
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
  if (faacv)
    free(faacv);
}

void printConf(faacAACStream *as)
{
  printf("AAC configuration:\n");
  printf("----------------------------------------------\n");
  printf("AAC profile: %s.\n", (as->profile==MAIN_PROFILE)?"MAIN":"LOW");
  printf("Bitrate: %dkbps.\n", as->bit_rate/1000);
  printf("Mid/Side (MS) stereo coding: %s.\n",
 	(as->use_MS==1)?"Always":((as->use_MS==0)?"Switching":"Off"));
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
  char *InFileNames[100], *OutFileNames[100];
  faacAACStream *as;

	/* System dependant types */
#ifdef _WIN32
  long	begin, end;
  int nTotSecs, nSecs;
  int nMins;
#else
  float	totalSecs;
  int mins;
#endif

  /* create main aacstream object */
  as =(faacAACStream *) malloc(sizeof(faacAACStream));

  /* Print version of FAAC */
  printVersion();

  /* Process command line params */
  if ((FileCount=parse_arg(argc, argv, as, InFileNames, OutFileNames)) < 1) return 0;

  /* Print configuration */
  printConf(as);

  /* Process input files */
  for (i = 0; i < FileCount; i++) {
    printf("0%\tBusy encoding %s.\r", InFileNames[i]);
#ifdef _WIN32
    begin = GetTickCount();
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
      printf("%.2f%%\tBusy encoding %s.\r", min((double)(currentFrame*100)/frames,100),InFileNames[i]);

    } while (result != F_FINISH);

    /* destroying internal data */
    faac_EncodeFree(as);
#ifdef _WIN32
    end = GetTickCount();
    nTotSecs = (end-begin)/1000;
    nMins = nTotSecs / 60;
    nSecs = nTotSecs - (60*nMins);
    printf("Encoding %s took:\t%d:%.2d\t\n", InFileNames[i], nMins, nSecs);
#else
    totalSecs = (float)(clock())/(float)CLOCKS_PER_SEC;
    mins = totalSecs/60;
    printf("Encoding %s took: %i min, %.2f sec.\n", InFileNames[i], mins, totalSecs - (60 * mins));
#endif
    if(InFileNames[i]) free(InFileNames[i]);
    if(OutFileNames[i]) free(OutFileNames[i]);
  }
  if (as) free (as);
//  exit(FNO_ERROR); // Uncomment to use profiling on *nix systems
  return FNO_ERROR;
}

#endif // end of not dll


