/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 * Copyright (C) 2026 Nils Schimmelmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "input.h"
#include <faac.h>
#include "resource.h"
#include "output.h"
#include "charset.h"
#include "encode_engine.h"

#define WM_USER_PROGRESS    (WM_USER + 101)
#define WM_USER_SESS_START  (WM_USER + 102)
#define WM_USER_LOG         (WM_USER + 103)
#define WM_USER_SUMMARY     (WM_USER + 104)
#define WM_USER_ENCODE_DONE (WM_USER + 105)

#define GUI_PROGRESS_RANGE       1024
#define GUI_PROGRESS_THROTTLE_MS 33

static HINSTANCE hInstance;
static HWND hwndTip;

/* Wide-char at the Win32 edge (dialog controls, file dialogs, drag-drop);
   converted to UTF-8 via win32_utf16_to_utf8() right before crossing into
   the UTF-8-only libfrontend layer (encode_options_t, wav_open_read(),
   get_output_filename()). Filenames outside the current ANSI code page
   can't round-trip through the narrow Win32 APIs, hence wide at the edge. */
static WCHAR inputFilename[_MAX_PATH];
static WCHAR outputFilename[_MAX_PATH];

static BOOL Encoding = FALSE;

typedef struct {
    HWND hWnd;
    WCHAR inputFilename[_MAX_PATH];
    WCHAR outputFilename[_MAX_PATH];
} encode_thread_param_t;

enum RateMode {
    RATEMODE_VBR = 0,
    RATEMODE_ABR = 1
};

static progress_info_t g_gui_progress;
static encode_session_info_t g_gui_sess_info;
static encode_summary_t g_gui_summary;
static char g_gui_log_msg[256];
static progress_throttle_t g_gui_throttle;
static bool g_gui_progress_posted = false;
static CRITICAL_SECTION g_cs_progress;

static void SetProgressBarPos(HWND hDlg, int control_id, int pos, int max_pos)
{
    HWND hProgress = GetDlgItem(hDlg, control_id);
    if (!hProgress) return;

    if (pos < max_pos)
    {
        SendMessage(hProgress, PBM_SETPOS, (WPARAM)(pos + 1), 0);
        SendMessage(hProgress, PBM_SETPOS, (WPARAM)pos, 0);
    }
    else
    {
        SendMessage(hProgress, PBM_SETRANGE32, 0, (LPARAM)(max_pos + 1));
        SendMessage(hProgress, PBM_SETPOS, (WPARAM)(max_pos + 1), 0);
        SendMessage(hProgress, PBM_SETPOS, (WPARAM)max_pos, 0);
        SendMessage(hProgress, PBM_SETRANGE32, 0, (LPARAM)max_pos);
    }
}

static BOOL SelectFileName(HWND hParent, WCHAR *filename, BOOL forReading)
{
    OPENFILENAMEW ofn;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hParent;
    ofn.hInstance = hInstance;
    ofn.nFilterIndex = 0;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    filename[0] = L'\0';
    ofn.lpstrFile = filename;
    ofn.nMaxFile = _MAX_PATH;

    if (forReading)
    {
        static const WCHAR filters[] =
            L"Wave Files (*.wav)\0*.wav\0"
            L"AIFF Files (*.aif;*.aiff;*.aifc)\0*.aif;*.aiff;*.aifc\0"
            L"AU Files (*.au)\0*.au\0"
            L"All Files (*.*)\0*.*\0\0";

        ofn.lpstrFilter = filters;
        ofn.lpstrDefExt = L"wav";
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrTitle = L"Select Source File";

        return GetOpenFileNameW(&ofn);
    }
    else
    {
        static const WCHAR filters[] =
            L"MPEG-4 Audio (*.m4a)\0*.m4a\0"
            L"AAC Files (*.aac)\0*.aac\0"
            L"All Files (*.*)\0*.*\0\0";

        ofn.lpstrFilter = filters;
        ofn.lpstrDefExt = L"m4a";
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
        ofn.lpstrTitle = L"Select Output File";

        return GetSaveFileNameW(&ofn);
    }
}

static void AddTip(HWND hDlg, int ctrlId, const char *text)
{
    TOOLINFO ti = { .cbSize = sizeof(ti) };
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = hDlg;
    ti.uId = (UINT_PTR)GetDlgItem(hDlg, ctrlId);
    ti.lpszText = (LPSTR)text;
    SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

static LRESULT GetComboData(HWND hWnd, int control_id, LRESULT default_val)
{
    HWND hCtrl = GetDlgItem(hWnd, control_id);
    LRESULT sel = SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
    LRESULT data = (sel != CB_ERR) ? SendMessage(hCtrl, CB_GETITEMDATA, (WPARAM)sel, 0) : CB_ERR;
    return (data != CB_ERR) ? data : default_val;
}

/* Sets the quality/bitrate label + edit box for the given rate mode, shared
   by WM_INITDIALOG's initial state and the IDC_RATEMODE CBN_SELCHANGE
   handler so the two don't drift out of sync. */
static void ApplyRateModeUI(HWND hWnd, int mode)
{
    char text[16];

    if (mode == RATEMODE_VBR)
    {
        SetDlgItemText(hWnd, IDC_QUALITYLABEL, "Quantizer\nquality");
        snprintf(text, sizeof(text), "%d", DEFAULT_QUANT_QUALITY);
    }
    else
    {
        SetDlgItemText(hWnd, IDC_QUALITYLABEL, "Bitrate\n(kbps)");
        snprintf(text, sizeof(text), "%d", DEFAULT_ABR_KBPS);
    }
    SetDlgItemText(hWnd, IDC_QUALITY, text);
}

static void AwakeDialogControls(HWND hWnd)
{
    char szTemp[64];
    pcmfile_t *infile = NULL;
    char *utf8_input = win32_utf16_to_utf8(inputFilename);

    if (!utf8_input || (infile = wav_open_read(utf8_input, 0)) == NULL)
    {
        free(utf8_input);
        return;
    }

    unsigned int sampleRate = infile->samplerate;
    unsigned int numChannels = infile->channels;

    wav_close(infile);

    SetDlgItemTextW(hWnd, IDC_INPUTFILENAME, inputFilename);

    char *utf8_output = get_output_filename(utf8_input, 1 /* GUI always defaults to MP4 */);
    free(utf8_input);
    if (utf8_output)
    {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_output, -1, NULL, 0);
        if (wlen > 0 && wlen <= _MAX_PATH)
            MultiByteToWideChar(CP_UTF8, 0, utf8_output, -1, outputFilename, wlen);
        free(utf8_output);
    }

    EnableWindow(GetDlgItem(hWnd, IDC_OUTPUTFILENAME), TRUE);
    EnableWindow(GetDlgItem(hWnd, IDC_SELECT_OUTPUTFILE), TRUE);

    SetDlgItemTextW(hWnd, IDC_OUTPUTFILENAME, outputFilename);

    snprintf(szTemp, sizeof(szTemp), "%uHz %uch", sampleRate, numChannels);
    SetDlgItemText(hWnd, IDC_INPUTPARAMS, szTemp);

    EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);
}

static bool GuiProgressCallback(const progress_info_t *info, void *user_data)
{
    HWND hWnd = (HWND)user_data;

    if (!Encoding)
        return false;

    EnterCriticalSection(&g_cs_progress);
    /* Throttle updates to ~30 Hz (33ms) to avoid message queue spamming. */
    bool should_update = info->is_final ||
                         progress_throttle_tick(&g_gui_throttle, info,
                                                 GUI_PROGRESS_THROTTLE_MS / 1000.0);
    bool do_post = false;
    if (should_update)
    {
        g_gui_progress = *info;
        if (!g_gui_progress_posted || info->is_final)
        {
            g_gui_progress_posted = true;
            do_post = true;
        }
    }
    LeaveCriticalSection(&g_cs_progress);

    if (do_post)
    {
        PostMessage(hWnd, WM_USER_PROGRESS, 0, 0);
    }

    return true;
}

static void GuiSessionStartCallback(const encode_session_info_t *info, void *user_data)
{
    HWND hWnd = (HWND)user_data;
    if (!info)
        return;

    EnterCriticalSection(&g_cs_progress);
    g_gui_sess_info = *info;
    LeaveCriticalSection(&g_cs_progress);

    PostMessage(hWnd, WM_USER_SESS_START, 0, 0);
}

static void GuiLogCallback(int level, const char *message, void *user_data)
{
    HWND hWnd = (HWND)user_data;
    (void)level;
    if (!message)
        return;

    EnterCriticalSection(&g_cs_progress);
    snprintf(g_gui_log_msg, sizeof(g_gui_log_msg), "%s", message);
    LeaveCriticalSection(&g_cs_progress);

    PostMessage(hWnd, WM_USER_LOG, 0, 0);
}

static void GuiSummaryCallback(const encode_summary_t *summary, void *user_data)
{
    HWND hWnd = (HWND)user_data;
    if (!summary)
        return;

    EnterCriticalSection(&g_cs_progress);
    g_gui_summary = *summary;
    LeaveCriticalSection(&g_cs_progress);

    PostMessage(hWnd, WM_USER_SUMMARY, 0, 0);
}

static DWORD WINAPI EncodeFile(LPVOID pParam)
{
    encode_thread_param_t *param = (encode_thread_param_t *)pParam;
    if (!param)
        return 1;

    HWND hWnd = param->hWnd;

    EnterCriticalSection(&g_cs_progress);
    progress_throttle_reset(&g_gui_throttle);
    LeaveCriticalSection(&g_cs_progress);

    char *utf8_input = win32_utf16_to_utf8(param->inputFilename);
    char *utf8_output = win32_utf16_to_utf8(param->outputFilename);
    free(param);

    encode_options_t opts;
    init_encode_options(&opts);

    opts.input_filename = utf8_input;
    opts.output_filename = utf8_output;
    opts.container_mp4 = utf8_output && is_mp4_filename(utf8_output);
    opts.overwrite = true;

    opts.object_type = (enum faac_object_type)GetComboData(hWnd, IDC_OBJECTTYPE, FAAC_OBJ_AUTO);

    {
        LRESULT mode = SendMessage(GetDlgItem(hWnd, IDC_JOINTMODE), CB_GETCURSEL, 0, 0);
        opts.joint_mode = (mode == CB_ERR) ? FAAC_JOINT_MIXED : (enum faac_joint_mode)mode;
    }

    opts.shortctl = (enum faac_shortctl_mode)GetComboData(hWnd, IDC_SHORTCTL, FAAC_SHORTCTL_NORMAL);

    opts.use_tns = IsDlgButtonChecked(hWnd, IDC_USETNS) == BST_CHECKED;
    opts.stream_format = opts.container_mp4 ? FAAC_STREAM_RAW : FAAC_STREAM_ADTS;

    char szTemp[256];
    GetDlgItemText(hWnd, IDC_QUALITY, szTemp, sizeof(szTemp));

    {
        LRESULT mode = GetComboData(hWnd, IDC_RATEMODE, RATEMODE_VBR);
        parse_quality_or_bitrate(szTemp, mode == RATEMODE_ABR, &opts);
    }

    GetDlgItemText(hWnd, IDC_PNS, szTemp, sizeof(szTemp));
    if (szTemp[0] != '\0')
    {
        int pns = atoi(szTemp);
        opts.pns_level = (pns < 0) ? 0 : ((pns > 10) ? 10 : (int8_t)pns);
    }
    else
    {
        opts.pns_level = -1;
    }

    if (IsDlgButtonChecked(hWnd, IDC_BWCTL) == BST_CHECKED)
    {
        GetDlgItemText(hWnd, IDC_BANDWIDTH, szTemp, sizeof(szTemp));
        int bw = atoi(szTemp);
        opts.bandwidth = (bw > 0) ? (uint32_t)bw : 0;
    }

    encode_callbacks_t cbs = {
        .progress_cb = GuiProgressCallback,
        .session_start_cb = GuiSessionStartCallback,
        .summary_cb = GuiSummaryCallback,
        .log_cb = GuiLogCallback,
        .user_data = hWnd
    };

    EnterCriticalSection(&g_cs_progress);
    g_gui_log_msg[0] = '\0';
    LeaveCriticalSection(&g_cs_progress);

    int status = (utf8_input && utf8_output)
        ? run_encoding_session_ext(&opts, &cbs)
        : ENCODE_ERROR;

    free(utf8_input);
    free(utf8_output);
    free_encode_options(&opts);

    PostMessage(hWnd, WM_USER_ENCODE_DONE, (WPARAM)status, 0);
    return 0;
}

static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg)
    {
    case WM_USER_PROGRESS:
        {
            progress_info_t info;
            EnterCriticalSection(&g_cs_progress);
            info = g_gui_progress;
            g_gui_progress_posted = false;
            LeaveCriticalSection(&g_cs_progress);

            if (info.total_input_samples > 0)
            {
                double ratio = (double)info.current_input_samples / (double)info.total_input_samples;
                if (ratio > 1.0) ratio = 1.0;
                if (ratio < 0.0) ratio = 0.0;

                int pos = (int)(ratio * (double)GUI_PROGRESS_RANGE);
                SetProgressBarPos(hWnd, IDC_PROGRESS, pos, GUI_PROGRESS_RANGE);

                char HeaderText[64];
                int percent = (int)(ratio * 100.0);
                snprintf(HeaderText, sizeof(HeaderText), "FAAC GUI: %d%%", percent);
                SetWindowText(hWnd, HeaderText);
            }

            char szTemp[256];
            double playingTime = (double)info.current_input_samples / (double)(info.sample_rate ? info.sample_rate : 1);
            snprintf(szTemp, sizeof(szTemp),
                "Playing time: %02d:%04.1f\tEncoding time: %02d:%04.1f\n"
                "Play/enc factor: %.2f\tEstimated time left: %02d:%04.1f",
                (int)playingTime / 60, (float)((int)(playingTime * 10.0) % 600) / 10.0f,
                (int)(info.time_elapsed_sec / 60.0), (float)((int)(info.time_elapsed_sec * 10.0) % 600) / 10.0f,
                (float)info.speed_factor,
                (int)info.eta_sec / 60, (float)((int)(info.eta_sec * 10.0) % 600) / 10.0f);

            SetDlgItemText(hWnd, IDC_TIME, szTemp);
            return TRUE;
        }

    case WM_USER_SESS_START:
        {
            encode_session_info_t info;
            EnterCriticalSection(&g_cs_progress);
            info = g_gui_sess_info;
            LeaveCriticalSection(&g_cs_progress);

            char szParams[128];
            const char *aot = (info.object_type == FAAC_OBJ_HE_AAC_V1) ? "HE-AAC v1" : "Low Complexity";
            snprintf(szParams, sizeof(szParams), "%uHz %uch | %s | Cutoff: %uHz",
                     info.sample_rate, info.num_channels, aot, info.bandwidth);
            SetDlgItemText(hWnd, IDC_INPUTPARAMS, szParams);
            return TRUE;
        }

    case WM_USER_LOG:
        {
            char log_msg[256];
            EnterCriticalSection(&g_cs_progress);
            snprintf(log_msg, sizeof(log_msg), "%s", g_gui_log_msg);
            LeaveCriticalSection(&g_cs_progress);

            if (log_msg[0] != '\0')
            {
                size_t len = strlen(log_msg);
                if (len > 0 && log_msg[len - 1] == '\n')
                    log_msg[len - 1] = '\0';
                wchar_t *wmsg = win32_utf8_to_utf16(log_msg);
                SetDlgItemTextW(hWnd, IDC_TIME, wmsg ? wmsg : L"");
                free(wmsg);
            }
            return TRUE;
        }

    case WM_USER_SUMMARY:
        {
            encode_summary_t sum;
            EnterCriticalSection(&g_cs_progress);
            sum = g_gui_summary;
            LeaveCriticalSection(&g_cs_progress);

            char szSummary[256];
            snprintf(szSummary, sizeof(szSummary),
                     "Encoded %u frames (%" PRIu64 " samples) | Avg bitrate: %u kbps | Max frame size: %u bytes",
                     sum.frame_count, sum.sample_count, sum.avg_bitrate, sum.max_frame_size);
            SetDlgItemText(hWnd, IDC_TIME, szSummary);
            return TRUE;
        }

    case WM_USER_ENCODE_DONE:
        {
            int status = (int)wParam;

            if (status == ENCODE_ERROR)
            {
                char err[300] = "Encoding failed!";
                EnterCriticalSection(&g_cs_progress);
                if (g_gui_log_msg[0] != '\0')
                {
                    snprintf(err, sizeof(err), "Encoding failed:\n%s", g_gui_log_msg);
                }
                LeaveCriticalSection(&g_cs_progress);

                wchar_t *werr = win32_utf8_to_utf16(err);
                if (werr)
                {
                    SetDlgItemTextW(hWnd, IDC_TIME, werr);
                    MessageBoxW(hWnd, werr, L"Error", MB_OK | MB_ICONSTOP);
                    free(werr);
                }
                else
                {
                    SetDlgItemText(hWnd, IDC_TIME, err);
                    MessageBox(hWnd, err, "Error", MB_OK | MB_ICONSTOP);
                }
                SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, 0, 0);
            }
            else if (status == ENCODE_SUCCESS)
            {
                SetProgressBarPos(hWnd, IDC_PROGRESS, GUI_PROGRESS_RANGE, GUI_PROGRESS_RANGE);
                SetWindowText(hWnd, "FAAC GUI: 100%");
                MessageBeep(MB_OK);
            }
            else /* ENCODE_CANCELLED */
            {
                SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, 0, 0);
                SetWindowText(hWnd, "FAAC GUI");
            }

            Encoding = FALSE;
            EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);
            SetDlgItemText(hWnd, IDOK, "Encode");
            return TRUE;
        }

    case WM_INITDIALOG:
        {
            faac_library_info libinfo = { .struct_size = sizeof(libinfo) };
            if (faac_get_library_info(&libinfo) == FAAC_OK)
            {
                char txt[128];
                snprintf(txt, sizeof(txt), "libfaac version %s", libinfo.version ? libinfo.version : "?");
                SetDlgItemText(hWnd, IDC_COMPILEDATE, txt);
            }
            else
            {
                MessageBox(hWnd, "Wrong libfaac version!", "FAAC", MB_OK | MB_ICONERROR);
                PostMessage(hWnd, WM_CLOSE, 0, 0);
            }
        }

        inputFilename[0] = L'\0';
        outputFilename[0] = L'\0';

        {
            HWND hRM = GetDlgItem(hWnd, IDC_RATEMODE);
            LRESULT idx;
            idx = SendMessage(hRM, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"VBR (Quality)");
            SendMessage(hRM, CB_SETITEMDATA, idx, (LPARAM)RATEMODE_VBR);
            idx = SendMessage(hRM, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"ABR (Bitrate)");
            SendMessage(hRM, CB_SETITEMDATA, idx, (LPARAM)RATEMODE_ABR);
            SendMessage(hRM, CB_SETCURSEL, 0, 0);
        }

        {
            HWND hOT = GetDlgItem(hWnd, IDC_OBJECTTYPE);
            LRESULT idx;
            idx = SendMessage(hOT, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"Auto");
            SendMessage(hOT, CB_SETITEMDATA, idx, (LPARAM)FAAC_OBJ_AUTO);
            idx = SendMessage(hOT, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"Low Complexity");
            SendMessage(hOT, CB_SETITEMDATA, idx, (LPARAM)FAAC_OBJ_LOW);
            idx = SendMessage(hOT, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"HE-AAC v1");
            SendMessage(hOT, CB_SETITEMDATA, idx, (LPARAM)FAAC_OBJ_HE_AAC_V1);
            SendMessage(hOT, CB_SETCURSEL, 0, 0);
        }

        SendMessage(GetDlgItem(hWnd, IDC_JOINTMODE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"None");
        SendMessage(GetDlgItem(hWnd, IDC_JOINTMODE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"M/S");
        SendMessage(GetDlgItem(hWnd, IDC_JOINTMODE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"IS");
        SendMessage(GetDlgItem(hWnd, IDC_JOINTMODE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"Mixed");
        SendMessage(GetDlgItem(hWnd, IDC_JOINTMODE), CB_SETCURSEL, 3, 0);

        {
            HWND hSC = GetDlgItem(hWnd, IDC_SHORTCTL);
            LRESULT idx;
            idx = SendMessage(hSC, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"Normal");
            SendMessage(hSC, CB_SETITEMDATA, idx, (LPARAM)FAAC_SHORTCTL_NORMAL);
            idx = SendMessage(hSC, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"No Short");
            SendMessage(hSC, CB_SETITEMDATA, idx, (LPARAM)FAAC_SHORTCTL_NOSHORT);
            idx = SendMessage(hSC, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"No Long");
            SendMessage(hSC, CB_SETITEMDATA, idx, (LPARAM)FAAC_SHORTCTL_NOLONG);
            SendMessage(hSC, CB_SETCURSEL, 0, 0);
        }

        CheckDlgButton(hWnd, IDC_USETNS, FALSE);
        ApplyRateModeUI(hWnd, RATEMODE_VBR);
        SetDlgItemText(hWnd, IDC_PNS, "4"); /* library default, libfaac/faac.c */
        SetDlgItemText(hWnd, IDC_BANDWIDTH, "0");

        hwndTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_BALLOON,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hWnd, NULL, hInstance, NULL);
        if (hwndTip)
        {
            SetWindowPos(hwndTip, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, 300); /* enables \n line breaks */

            AddTip(hWnd, IDC_QUALITY,
                "Percent (VBR) or kbps/channel (ABR), depending on Rate Mode.");
            AddTip(hWnd, IDC_BWCTL,
                "Cap the encoded bandwidth to a specific frequency instead of\n"
                "letting the encoder choose it automatically.");
            AddTip(hWnd, IDC_BANDWIDTH, "Cutoff frequency in Hz.");
            AddTip(hWnd, IDC_RATEMODE,
                "VBR (Quality) targets a quality level; ABR (Bitrate) targets\n"
                "an average bitrate.");
            AddTip(hWnd, IDC_OBJECTTYPE,
                "Auto picks LC or HE-AAC v1 based on bitrate; force one to\n"
                "override that choice.");
            AddTip(hWnd, IDC_USETNS, "Temporal Noise Shaping: reduces pre-echo on transients.");
            AddTip(hWnd, IDC_PNS, "Perceptual Noise Substitution level, 0-10; 0 disables it.");
            AddTip(hWnd, IDC_SHORTCTL,
                "Normal switches block length automatically; No Short/No Long\n"
                "force one block type throughout the encode.");
            AddTip(hWnd, IDC_JOINTMODE,
                "None: independent channels. M/S and IS: fixed stereo coding.\n"
                "Mixed: chooses dynamically per band (default).");
        }

        DragAcceptFiles(hWnd, TRUE);
        return TRUE;

    case WM_DROPFILES:
        if (!Encoding && DragQueryFileW((HDROP)wParam, 0, inputFilename, _MAX_PATH - 1))
            AwakeDialogControls(hWnd);

        DragFinish((HDROP)wParam);
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (!Encoding)
            {
                encode_thread_param_t *param = (encode_thread_param_t *)malloc(sizeof(encode_thread_param_t));
                if (param)
                {
                    param->hWnd = hWnd;
                    GetDlgItemTextW(hWnd, IDC_INPUTFILENAME, param->inputFilename, _MAX_PATH);
                    GetDlgItemTextW(hWnd, IDC_OUTPUTFILENAME, param->outputFilename, _MAX_PATH);

                    SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, GUI_PROGRESS_RANGE));
                    SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, 0, 0);

                    Encoding = TRUE;
                    SetDlgItemText(hWnd, IDOK, "Stop");

                    DWORD retval;
                    HANDLE hThread = CreateThread(NULL, 0, EncodeFile, param, 0, &retval);
                    if (hThread)
                    {
                        CloseHandle(hThread);
                    }
                    else
                    {
                        Encoding = FALSE;
                        SetDlgItemText(hWnd, IDOK, "Encode");
                        free(param);
                    }
                }
            }
            else
            {
                /* User clicked Stop: signal worker thread to cancel, then disable button until thread finishes */
                Encoding = FALSE;
                EnableWindow(GetDlgItem(hWnd, IDOK), FALSE);
                SetDlgItemText(hWnd, IDOK, "Stopping...");
            }
            return TRUE;

        case IDCANCEL:
            EndDialog(hWnd, TRUE);
            return TRUE;

        case IDC_SELECT_INPUTFILE:
            if (!Encoding && SelectFileName(hWnd, inputFilename, TRUE))
                AwakeDialogControls(hWnd);
            break;

        case IDC_SELECT_OUTPUTFILE:
            if (!Encoding && SelectFileName(hWnd, outputFilename, FALSE))
            {
                SetDlgItemTextW(hWnd, IDC_OUTPUTFILENAME, outputFilename);
            }
            break;

        case IDC_BWCTL:
            switch (IsDlgButtonChecked(hWnd, IDC_BWCTL))
            {
            case BST_CHECKED:
                EnableWindow(GetDlgItem(hWnd, IDC_BANDWIDTH), TRUE);
                break;
            case BST_UNCHECKED:
                EnableWindow(GetDlgItem(hWnd, IDC_BANDWIDTH), FALSE);
                break;
            }
            break;

        case IDC_RATEMODE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                LRESULT mode = GetComboData(hWnd, IDC_RATEMODE, RATEMODE_VBR);
                ApplyRateModeUI(hWnd, (int)mode);
            }
            break;
        }
        break;
    }

    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    hInstance = hInst;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);
    InitializeCriticalSection(&g_cs_progress);
    int res = (int)DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, DialogProc);
    DeleteCriticalSection(&g_cs_progress);
    return res;
}
