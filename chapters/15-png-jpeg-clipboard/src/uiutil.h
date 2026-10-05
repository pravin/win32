/* DrawLite - Win32 Tutorial
 * Chapter 15 - PNG, JPEG and the clipboard
 *
 * File: uiutil.h
 * Small helpers for dealing with fonts and high DPI screens.
 */
#ifndef UIUTIL_H
#define UIUTIL_H

#include <windows.h>

// Function: Ui_Scale
// Converts a size designed for a 96 DPI screen into pixels for the given DPI.
// At 96 DPI (100%) nothing changes. At 144 DPI (150%) 10 becomes 15.
int Ui_Scale(UINT dpi, int pixels);

// Function: Ui_CreateFont
// Creates the standard Windows UI font (Segoe UI) for the given DPI.
// The caller owns the font and must DeleteObject it.
HFONT Ui_CreateFont(UINT dpi);

// Function: Ui_SetFontOnChildren
// Sends WM_SETFONT to every child window of hwnd (and their children).
void Ui_SetFontOnChildren(HWND hwnd, HFONT font);

#endif // UIUTIL_H
