/* DrawLite - Win32 Tutorial
 * Chapter 17 - Layers panel and blend modes
 *
 * File: namedlg.c
 */

#include <windows.h>
#include "namedlg.h"
#include "resource.h"

typedef struct NameDlgData
{
    wchar_t *name;
    int capacity;
} NameDlgData;

static INT_PTR CALLBACK
NameDlg_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    NameDlgData *data;

    switch (msg)
    {
    case WM_INITDIALOG:
        data = (NameDlgData *)lParam;
        SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
        SendDlgItemMessageW(hDlg, IDC_NAME_EDIT, EM_SETLIMITTEXT, (WPARAM)(data->capacity - 1), 0);
        SetDlgItemTextW(hDlg, IDC_NAME_EDIT, data->name);
        SendDlgItemMessageW(hDlg, IDC_NAME_EDIT, EM_SETSEL, 0, -1);   // Select it all, ready to be typed over
        return TRUE;    // Returning TRUE lets the dialog manager set focus to the first control

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            data = (NameDlgData *)GetWindowLongPtrW(hDlg, DWLP_USER);
            GetDlgItemTextW(hDlg, IDC_NAME_EDIT, data->name, data->capacity);
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL
NameDlg_Show(HINSTANCE hInstance, HWND hParent, wchar_t *name, int capacity)
{
    NameDlgData data;

    data.name = name;
    data.capacity = capacity;
    return DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_LAYER_NAME), hParent, NameDlg_Proc, (LPARAM)&data) == IDOK;
}
