/* DrawLite - Win32 Tutorial
 * Chapter 8 - Pencil and brush
 *
 * File: mainwindow.h
 * The main application window.
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include "doc.h"
#include "tools.h"

// Struct: MainWindow
// Everything the main window needs to remember.
typedef struct MainWindow
{
    HINSTANCE hInstance;    // The application instance
    HWND hwnd;              // The window itself
    HWND hToolbar;          // Toolbar along the top
    HWND hStatusbar;        // Status bar along the bottom
    HWND hPalette;          // Tool palette down the left
    HWND hCanvas;           // The drawing area
    HFONT hFont;            // Font used by the controls. We own it.
    ToolSettings settings;  // Selected tool, colours and brush options
    Document *doc;          // The picture being edited
    int newWidth;           // Size suggested in the New Image dialog
    int newHeight;
} MainWindow;

// Function: MainWindow_Create
// Registers the window class (once), creates the window and shows it.
//
// Returns:
//   A new MainWindow, or NULL on failure. Free it with MainWindow_Destroy.
MainWindow *MainWindow_Create(HINSTANCE hInstance, int nCmdShow);

// Function: MainWindow_Destroy
// Frees the memory used by a MainWindow.
void MainWindow_Destroy(MainWindow *mw);

// Function: MainWindow_GetHwnd
// Returns the HWND of the window.
HWND MainWindow_GetHwnd(const MainWindow *mw);

#endif // MAINWINDOW_H
