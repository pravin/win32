/* DrawLite - Win32 Tutorial
 * Chapter 13 - Undo and redo
 *
 * File: newdlg.h
 * The New Image dialog.
 */
#ifndef NEWDLG_H
#define NEWDLG_H

#include <windows.h>

// Function: NewDlg_Show
// Asks the user for the size of a new image.
//
// Parameters:
//   width, height - In: the sizes to start with. Out: what the user chose.
//
// Returns:
//   TRUE if the user pressed OK, FALSE if they cancelled.
BOOL NewDlg_Show(HINSTANCE hInstance, HWND hParent, int *width, int *height);

#endif // NEWDLG_H
