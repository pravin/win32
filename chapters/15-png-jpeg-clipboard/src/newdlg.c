/* DrawLite - Win32 Tutorial
 * Chapter 15 - PNG, JPEG and the clipboard
 *
 * File: newdlg.c
 */

#include <windows.h>
#include "newdlg.h"
#include "resource.h"

#define NEW_MIN_SIZE 1
#define NEW_MAX_SIZE 8192

typedef struct NewDlgData
{
    int width;
    int height;
} NewDlgData;

static INT_PTR CALLBACK NewDlg_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

BOOL
NewDlg_Show(HINSTANCE hInstance, HWND hParent, int *width, int *height)
{
    NewDlgData data;
    INT_PTR result;

    data.width = *width;
    data.height = *height;

    // The last parameter arrives as lParam in WM_INITDIALOG
    result = DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_NEWIMAGE), hParent, NewDlg_Proc, (LPARAM)&data);
    if (result != IDOK)
        return FALSE;

    *width = data.width;
    *height = data.height;
    return TRUE;
}

static INT_PTR CALLBACK
NewDlg_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    NewDlgData *data;

    switch (msg)
    {
    case WM_INITDIALOG:
        // Dialogs have a spare pointer-sized slot for us (DWLP_USER)
        SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
        data = (NewDlgData *)lParam;
        SetDlgItemInt(hDlg, IDC_NEW_WIDTH, (UINT)data->width, FALSE);
        SetDlgItemInt(hDlg, IDC_NEW_HEIGHT, (UINT)data->height, FALSE);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            {
                BOOL okW, okH;
                UINT w = GetDlgItemInt(hDlg, IDC_NEW_WIDTH, &okW, FALSE);
                UINT h = GetDlgItemInt(hDlg, IDC_NEW_HEIGHT, &okH, FALSE);

                if (!okW || !okH || w < NEW_MIN_SIZE || w > NEW_MAX_SIZE || h < NEW_MIN_SIZE || h > NEW_MAX_SIZE)
                {
                    MessageBoxW(hDlg, L"Please enter sizes between 1 and 8192 pixels.", L"New Image",
                        MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                data = (NewDlgData *)GetWindowLongPtrW(hDlg, DWLP_USER);
                data->width = (int)w;
                data->height = (int)h;
                EndDialog(hDlg, IDOK);
                return TRUE;
            }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
