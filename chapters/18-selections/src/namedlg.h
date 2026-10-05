/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 *
 * File: namedlg.h
 * A small dialog that asks for a layer name.
 */
#ifndef NAMEDLG_H
#define NAMEDLG_H

#include <windows.h>

// Function: NameDlg_Show
// Asks the user to edit a name.
//
// Parameters:
//   name     - In: the name to start with. Out: what the user typed.
//   capacity - Size of name, in characters, including the terminating zero.
//
// Returns:
//   TRUE if the user pressed OK.
BOOL NameDlg_Show(HINSTANCE hInstance, HWND hParent, wchar_t *name, int capacity);

#endif // NAMEDLG_H
