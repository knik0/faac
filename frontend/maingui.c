/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: maingui.c,v 1.10 2001/03/06 21:02:33 menno Exp $
 */

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdlib.h>

#include <sndfile.h>  /* http://www.zip.com.au/~erikd/libsndfile/ */

#include "faac.h"
#include "resource.h"

#define PCMBUFSIZE 1024
#define BITBUFSIZE 8192

static HINSTANCE hInstance;

static char inputFilename[_MAX_PATH], outputFilename[_MAX_PATH];

static BOOL Encoding = FALSE;

static BOOL SelectFileName(HWND hParent, char *filename, BOOL forReading)
{
	OPENFILENAME ofn;

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hParent;
	ofn.hInstance = hInstance;
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 31;
	filename [0] = 0x00;
	ofn.lpstrFile = (LPSTR)filename;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (forReading)
	{
		char filters[] = { "Wave Files (*.wav)\0*.wav\0" \
			"AIFF Files (*.aif;*.aiff;*.aifc)\0*.aif;*.aiff;*.aifc\0" \
			"AU Files (*.au)\0*.au\0" \
			"All Files (*.*)\0*.*\0\0" };

		ofn.lpstrFilter = filters;
		ofn.lpstrDefExt = "wav";

		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		ofn.lpstrTitle = "Select Source File";

		return GetOpenFileName (&ofn);
	} else {
		char filters [] = { "AAC Files (*.aac)\0*.aac\0" \
			"All Files (*.*)\0*.*\0\0" };

		ofn.lpstrFilter = filters;
		ofn.lpstrDefExt = "aac";

		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
		ofn.lpstrTitle = "Select Output File";

		return GetSaveFileName(&ofn);
	}
}

static void AwakeDialogControls(HWND hWnd)
{
	char szTemp[64];
	SNDFILE *infile;
	SF_INFO sfinfo;
	unsigned int sampleRate, numChannels;
	char *pExt;

	if ((infile = sf_open_read(inputFilename, &sfinfo)) == NULL)
		return;

	/* determine input file parameters */
	sampleRate = sfinfo.samplerate;
	numChannels = sfinfo.channels;

	sf_close(infile);

	SetDlgItemText (hWnd, IDC_INPUTFILENAME, inputFilename);

	strncpy(outputFilename, inputFilename, sizeof(outputFilename) - 5);

	pExt = strrchr(outputFilename, '.');

	if (pExt == NULL) lstrcat(outputFilename, ".aac");
	else lstrcpy(pExt, ".aac");

	EnableWindow(GetDlgItem(hWnd, IDC_OUTPUTFILENAME), TRUE);
	EnableWindow(GetDlgItem(hWnd, IDC_SELECT_OUTPUTFILE), TRUE);

	SetDlgItemText(hWnd, IDC_OUTPUTFILENAME, outputFilename);

	wsprintf(szTemp, "%iHz %ich", sampleRate, numChannels);
	SetDlgItemText(hWnd, IDC_INPUTPARAMS, szTemp);

	EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);
}

static DWORD WINAPI EncodeFile(LPVOID pParam)
{
	HWND hWnd = (HWND) pParam;
	SNDFILE *infile;
	SF_INFO sfinfo;

	GetDlgItemText(hWnd, IDC_INPUTFILENAME, inputFilename, sizeof(inputFilename));
	GetDlgItemText(hWnd, IDC_OUTPUTFILENAME, outputFilename, sizeof(outputFilename));

	/* open the input file */
	if ((infile = sf_open_read(inputFilename, &sfinfo)) != NULL)
	{
		/* determine input file parameters */
		unsigned int sampleRate = sfinfo.samplerate;
		unsigned int numChannels = sfinfo.channels;

		/* open and setup the encoder */
		faacEncHandle hEncoder = faacEncOpen(sampleRate, numChannels);
		if (hEncoder)
		{
			HANDLE hOutfile;
			char szTemp[64];

			/* set encoder configuration */
			faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(hEncoder);

			config->allowMidside = IsDlgButtonChecked(hWnd, IDC_ALLOWMIDSIDE) == BST_CHECKED ? 1 : 0;
			config->useTns = IsDlgButtonChecked(hWnd, IDC_USETNS) == BST_CHECKED ? 1 : 0;
			config->useLfe = IsDlgButtonChecked(hWnd, IDC_USELFE) == BST_CHECKED ? 1 : 0;
			config->useLtp = IsDlgButtonChecked(hWnd, IDC_USELTP) == BST_CHECKED ? 1 : 0;
			config->aacProfile = IsDlgButtonChecked(hWnd, IDC_LC) == BST_CHECKED ? LOW : 0;
			config->aacProfile = IsDlgButtonChecked(hWnd, IDC_MAIN) == BST_CHECKED ? MAIN : 0;
			config->aacProfile = IsDlgButtonChecked(hWnd, IDC_SSR) == BST_CHECKED ? SSR : 0;
			GetDlgItemText(hWnd, IDC_BITRATE, szTemp, sizeof(szTemp));
			config->bitRate = atoi(szTemp);
			GetDlgItemText(hWnd, IDC_BANDWIDTH, szTemp, sizeof(szTemp));
			config->bandWidth = atoi(szTemp);

			if (!faacEncSetConfiguration(hEncoder, config))
			{
				faacEncClose(hEncoder);
				sf_close(infile);

				MessageBox (hWnd, "faacEncSetConfiguration failed!", "Error", MB_OK | MB_ICONSTOP);

				SendMessage(hWnd,WM_SETTEXT,0,(long)"FAAC GUI");
				Encoding = FALSE;
				SetDlgItemText(hWnd, IDOK, "Encode");

				return 0;
			}

			/* open the output file */
			hOutfile = CreateFile(outputFilename, GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hOutfile != INVALID_HANDLE_VALUE)
			{
				UINT startTime = GetTickCount(), lastUpdated = 0;
				DWORD totalBytesRead = 0;

				unsigned int bytesInput = 0, bytesConsumed = 0;
				DWORD numberOfBytesWritten = 0;
				short *pcmbuf;
				unsigned char *bitbuf;
				char HeaderText[50];
				char Percentage[5];

				pcmbuf = (short*)malloc(PCMBUFSIZE*numChannels*sizeof(short));
				bitbuf = (unsigned char*)malloc(BITBUFSIZE*sizeof(unsigned char));

				SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 1024));
				SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, 0, 0);

				for ( ;; )
				{
					int bytesWritten;
					int samplesToRead = PCMBUFSIZE;
					UINT timeElapsed, timeEncoded;

					bytesInput = sf_read_short(infile, pcmbuf, numChannels*PCMBUFSIZE) * sizeof(short);
					
					SendDlgItemMessage (hWnd, IDC_PROGRESS, PBM_SETPOS, (unsigned long)((float)totalBytesRead * 1024.0f / (sfinfo.samples*2*numChannels)), 0);
					
					/* Percentage for Dialog Output */
					_itoa((int)((float)totalBytesRead * 100.0f / (sfinfo.samples*2*numChannels)),Percentage,10);
					lstrcpy(HeaderText,"FAAC GUI: ");
					lstrcat(HeaderText,Percentage);
					lstrcat(HeaderText,"%");
					SendMessage(hWnd,WM_SETTEXT,0,(long)HeaderText);
					
					totalBytesRead += bytesInput;

					timeElapsed = (GetTickCount () - startTime) / 1000;
					timeEncoded = totalBytesRead / (sampleRate * numChannels * sizeof (short));

					if (timeElapsed > lastUpdated)
					{
						float factor;

						lastUpdated = timeElapsed;

						factor = (float) timeEncoded / (float) (timeElapsed ? timeElapsed : 1);

						sprintf(szTemp, "Playing time: %2.2i:%2.2i:%2.2i  Encoding time: %2.2i:%2.2i:%2.2i  Factor: %.1f",
							timeEncoded / 3600, (timeEncoded % 3600) / 60, timeEncoded % 60,
							timeElapsed / 3600, (timeElapsed % 3600) / 60, timeElapsed % 60,
							factor);

						SetDlgItemText(hWnd, IDC_TIME, szTemp);
					}

					/* call the actual encoding routine */
					bytesWritten = faacEncEncode(hEncoder,
						pcmbuf,
						bytesInput/2,
						bitbuf,
						BITBUFSIZE);

					/* Stop Pressed */
					if ( !Encoding ) 
						break;

					/* all done, bail out */
					if (!bytesInput && !bytesWritten)
						break;

					if (bytesWritten < 0)
					{
						MessageBox (hWnd, "faacEncEncodeFrame failed!", "Error", MB_OK | MB_ICONSTOP);
						break;
					}

					WriteFile(hOutfile, bitbuf, bytesWritten, &numberOfBytesWritten, NULL);
				
					}

				CloseHandle(hOutfile);
				if (pcmbuf) free(pcmbuf);
				if (bitbuf) free(bitbuf);
			}

			faacEncClose(hEncoder);
		}

		sf_close(infile);
		MessageBeep(1);

		SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, 0, 0);
	} else {
		MessageBox(hWnd, "Couldn't open input file!", "Error", MB_OK | MB_ICONSTOP);
	}

	SendMessage(hWnd,WM_SETTEXT,0,(long)"FAAC GUI");
	Encoding = FALSE;
	SetDlgItemText(hWnd, IDOK, "Encode");
	return 0;
}

static BOOL WINAPI DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
    case WM_INITDIALOG:

		inputFilename [0] = 0x00;

		CheckDlgButton(hWnd, IDC_MAIN, TRUE);
		CheckDlgButton(hWnd, IDC_ALLOWMIDSIDE, TRUE);
		CheckDlgButton(hWnd, IDC_USELFE, FALSE);
		CheckDlgButton(hWnd, IDC_USETNS, TRUE);
		CheckDlgButton(hWnd, IDC_USELTP, TRUE);
		SetDlgItemText(hWnd, IDC_BITRATE, "64000");
		SetDlgItemText(hWnd, IDC_BANDWIDTH, "18000");

		SetDlgItemText(hWnd, IDC_COMPILEDATE, "Ver: " __DATE__);

		DragAcceptFiles(hWnd, TRUE);
		return TRUE;

    case WM_DROPFILES:

		if (DragQueryFile((HDROP) wParam, 0, (LPSTR) inputFilename, _MAX_PATH - 1))
			AwakeDialogControls(hWnd);
		
		DragFinish((HDROP) wParam);
		return FALSE;

    case WM_COMMAND:

		switch (wParam)
		{
        case IDOK:
			
			if ( !Encoding ) 
			{
				int retval;
				CreateThread(NULL,0,EncodeFile,hWnd,0,&retval);
				Encoding = TRUE;
				SetDlgItemText(hWnd, IDOK, "Stop");
			}
			else
			{
				Encoding = FALSE;
				SetDlgItemText(hWnd, IDOK, "Encode");
			}
			return TRUE;

        case IDCANCEL:

			EndDialog(hWnd, TRUE);
			return TRUE;

        case IDC_SELECT_INPUTFILE:

			if (SelectFileName(hWnd, inputFilename, TRUE))
				AwakeDialogControls(hWnd);

			break;

        case IDC_SELECT_OUTPUTFILE:

			if (SelectFileName(hWnd, outputFilename, FALSE))
			{
				SetDlgItemText(hWnd, IDC_OUTPUTFILENAME, outputFilename);
			}

			break;
		}

		break;
	}

	return FALSE;
}

int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	hInstance = hInst;

	return DialogBox(hInstance, MAKEINTRESOURCE (IDD_MAINDIALOG), NULL, (DLGPROC) DialogProc);
}
