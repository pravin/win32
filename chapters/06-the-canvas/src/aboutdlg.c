/* DrawLite - Win32 Tutorial
 * Chapter 6 - The canvas
 *
 * File: aboutdlg.c
 * The About box.
 */

#include <windows.h>
#include "aboutdlg.h"
#include "resource.h"

static INT_PTR CALLBACK AboutDlg_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void
AboutDlg_Show(HINSTANCE hInstance, HWND hParent)
{
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_ABOUT), hParent, AboutDlg_Proc, 0);
}

// Function: AboutDlg_Proc
// Handles messages for the About box. Dialog procs return TRUE if they
// handled a message and FALSE if they did not.
static INT_PTR CALLBACK
AboutDlg_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
