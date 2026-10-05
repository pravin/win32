/* DrawLite - Win32 Tutorial
 * Chapter 9 - Colour
 *
 * File: uiutil.c
 */

#include <windows.h>
#include "uiutil.h"

int
Ui_Scale(UINT dpi, int pixels)
{
    return MulDiv(pixels, (int)dpi, 96);
}

HFONT
Ui_CreateFont(UINT dpi)
{
    NONCLIENTMETRICSW ncm;
    ZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);

    // Ask Windows which font it uses for message boxes at this DPI
    if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi))
        return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    return CreateFontIndirectW(&ncm.lfMessageFont);
}

static BOOL CALLBACK
SetFontProc(HWND child, LPARAM lParam)
{
    SendMessageW(child, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

void
Ui_SetFontOnChildren(HWND hwnd, HFONT font)
{
    EnumChildWindows(hwnd, SetFontProc, (LPARAM)font);
}
