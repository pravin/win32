/* DrawLite - Win32 Tutorial
 * Chapter 4 - Housekeeping
 *
 * File: mainwindow.h
 * The main application window.
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>

// Struct: MainWindow
// Everything the main window needs to remember. Callers should treat this
// as opaque and use the MainWindow_ functions instead of poking inside.
typedef struct MainWindow
{
    HINSTANCE hInstance;    // The application instance
    HWND hwnd;              // The window itself
} MainWindow;

// Function: MainWindow_Create
// Registers the window class (once), creates the window and shows it.
//
// Parameters:
//   hInstance - Instance handle from wWinMain
//   nCmdShow  - How the window should be shown (SW_SHOW, SW_MAXIMIZE, ...)
//
// Returns:
//   A new MainWindow, or NULL on failure. Free it with MainWindow_Destroy.
MainWindow *MainWindow_Create(HINSTANCE hInstance, int nCmdShow);

// Function: MainWindow_Destroy
// Frees the memory used by a MainWindow. The window itself is
// already gone by the time the message loop ends.
void MainWindow_Destroy(MainWindow *mw);

// Function: MainWindow_GetHwnd
// Returns the HWND of the window.
HWND MainWindow_GetHwnd(const MainWindow *mw);

#endif // MAINWINDOW_H
