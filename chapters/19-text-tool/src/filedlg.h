/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: filedlg.h
 * The standard Open and Save As dialogs.
 */
#ifndef FILEDLG_H
#define FILEDLG_H

#include <windows.h>

// Function: FileDlg_Open
// Shows the Open dialog. On OK, copies the chosen path into path (which must
// hold MAX_PATH characters) and returns TRUE.
BOOL FileDlg_Open(HWND owner, wchar_t *path);

// Function: FileDlg_SaveAs
// Shows the Save As dialog. path holds the suggested name on the way in and
// the chosen path on the way out. Adds ".bmp" if the user left it off.
BOOL FileDlg_SaveAs(HWND owner, wchar_t *path);

#endif // FILEDLG_H
