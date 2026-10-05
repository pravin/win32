/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
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
#define OPEN_FILTER L"All pictures\0*.dlt;*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0" \
                    L"DrawLite (*.dlt)\0*.dlt\0PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg;*.jpeg\0" \
                    L"Bitmap (*.bmp)\0*.bmp\0All files (*.*)\0*.*\0"
#define SAVE_FILTER L"DrawLite, with layers (*.dlt)\0*.dlt\0PNG (*.png)\0*.png\0" \
                    L"JPEG (*.jpg)\0*.jpg;*.jpeg\0Bitmap (*.bmp)\0*.bmp\0"

BOOL
FileDlg_Open(HWND owner, wchar_t *path)
{
    OPENFILENAMEW ofn;

    path[0] = L'\0';
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = OPEN_FILTER;
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
    ofn.lpstrFilter = SAVE_FILTER;
    ofn.nFilterIndex = 1;               // Starts on DrawLite. Filter numbers count from 1.
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Save Image As";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn))
        return FALSE;

    // If they typed a name with no extension, add the one that goes with the
    // filter they had selected. (lpstrDefExt could do this, but only with one
    // fixed extension, whatever the filter says.)
    {
        static const wchar_t *exts[] = { L".dlt", L".png", L".jpg", L".bmp" };
        const wchar_t *p, *dot = NULL;

        for (p = path; *p; p++)
        {
            if (*p == L'\\' || *p == L'/')
                dot = NULL;
            else if (*p == L'.')
                dot = p;
        }
        if (!dot && ofn.nFilterIndex >= 1 && ofn.nFilterIndex <= 4)
        {
            StringCchCatW(path, MAX_PATH, exts[ofn.nFilterIndex - 1]);
            // The dialog's own overwrite warning checked the name without the
            // extension, so we have to ask again ourselves.
            if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES
                && MessageBoxW(owner, L"That file already exists. Replace it?", L"Save Image As",
                    MB_YESNO | MB_ICONWARNING) != IDYES)
                return FALSE;
        }
    }
    return TRUE;
}
