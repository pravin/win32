/* DrawLite - Win32 Tutorial
 * Chapter 14 - Open and save
 *
 * File: aboutdlg.h
 * The About box.
 */
#ifndef ABOUTDLG_H
#define ABOUTDLG_H

#include <windows.h>

// Function: AboutDlg_Show
// Shows the About box and waits until it is closed.
//
// Parameters:
//   hInstance - Instance that holds the dialog resource
//   hParent   - The window that owns the dialog
void AboutDlg_Show(HINSTANCE hInstance, HWND hParent);

#endif // ABOUTDLG_H
