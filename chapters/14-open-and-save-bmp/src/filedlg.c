/* DrawLite - Win32 Tutorial
 * Chapter 14 - Open and save
 *
 * File: filedlg.c
 */

#include <windows.h>
#include <commdlg.h>
#include <strsafe.h>
#include "filedlg.h"

// A filter is a list of pairs: the text the user sees, then the pattern.
// Each piece ends with a \0, and the whole list ends with an extra \0.
// That is why this is not a normal string, and why we cannot use strlen on it.
#define BMP_FILTER  L"Bitmap files (*.bmp)\0*.bmp\0All files (*.*)\0*.*\0"

BOOL
FileDlg_Open(HWND owner, wchar_t *path)
{
    OPENFILENAMEW ofn;

    path[0] = L'\0';
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = BMP_FILTER;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Open Image";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&ofn);
}

BOOL
FileDlg_SaveAs(HWND owner, wchar_t *path)
{
    OPENFILENAMEW ofn;

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = BMP_FILTER;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Save Image As";
    ofn.lpstrDefExt = L"bmp";           // Added if the user does not type an extension
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetSaveFileNameW(&ofn);
}
