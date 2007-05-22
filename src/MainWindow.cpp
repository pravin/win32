/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (March 08, 2007)
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>
#include <windowsx.h>
#include "MainWindow.h"
#include "AboutDialog.h"
#include "resource.h"

char MainWindow::m_szClassName[] = "DrawLite";
HINSTANCE MainWindow::m_hInstance = NULL;

// Constructor: MainWindow
// Initializes the WNDCLASS
MainWindow::MainWindow(HINSTANCE hInstance)
{
    m_hInstance = hInstance; // Save Instance handle

    m_wndClass.cbSize = sizeof(WNDCLASSEX); // Must always be sizeof(WNDCLASSEX)
    m_wndClass.style = CS_DBLCLKS; // Class styles
    m_wndClass.lpfnWndProc = MainWndProc; // Pointer to callback procedure
    m_wndClass.cbClsExtra = 0; // Extra bytes to allocate following the wndclassex structure
    m_wndClass.cbWndExtra = 0; // Extra bytes to allocate following an instance of the structure
    m_wndClass.hInstance = hInstance; // Instance of the application
    m_wndClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON)); // Class Icon
    m_wndClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Class cursor
    m_wndClass.hbrBackground = (HBRUSH) (COLOR_WINDOW); // Background brush
    m_wndClass.lpszMenuName = MAKEINTRESOURCE(IDM_MAINMENU); // Menu Resource
    m_wndClass.lpszClassName = m_szClassName; // Name of this class
    m_wndClass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON)); // Small icon for this class
}

// Destructor: MainWindow
// Frees up resources used by this instance
MainWindow::~MainWindow()
{
}

// Function: MainWndProc
// The window proc for the MainWindow Class
LRESULT CALLBACK MainWindow::MainWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage (0);
        break;
    case WM_COMMAND:
        HANDLE_WM_COMMAND(hwnd, wParam, lParam, OnCommand);
        break;
    default:
        return DefWindowProc (hwnd, msg, wParam, lParam);
    }

    return 0;
}

// Function: OnCommand
// Handles WM_COMMAND messages (Menu, toolbar, etc)
void MainWindow::OnCommand(HWND hwnd, int id, HWND hCtl, UINT codeNotify)
{
    switch(id)
    {
    case IDM_FILE_EXIT:
        PostQuitMessage(0);
        break;
    case IDM_HELP_ABOUT:
        AboutDialog* dlg = new AboutDialog();
        dlg->Run(m_hInstance, hwnd);
        delete dlg; dlg = NULL;
        break;
    }
}

// Function: Run
// Creates the window and displays it
// Parameters:
//   nCmdShow - one of SW_SHOW, SW_HIDE, SW_MAXIMIZE, SW_MINIMIZE
// Returns:
//   true on success, false otherwise
bool MainWindow::Run(int nCmdShow)
{
    if(!RegisterClassEx(&m_wndClass))
        return false;
    m_hwnd = CreateWindowEx(
            0,
            m_szClassName,
            "Draw Lite",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            500,
            400,
            NULL,
            NULL,
            m_hInstance,
            NULL
            );
    if(!m_hwnd)
        return false;
    ShowWindow(m_hwnd, nCmdShow);
    return true;
}
