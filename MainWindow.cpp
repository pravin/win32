/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (March 08, 2007)
 * http://www.dustyant.com/wintut
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>
#include "MainWindow.h"

char MainWindow::m_szClassName[] = "DrawLite";

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
    m_wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Class Icon
    m_wndClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Class cursor
    m_wndClass.hbrBackground = (HBRUSH) (COLOR_WINDOW); // Background brush
    m_wndClass.lpszMenuName = NULL; // Menu Resource
    m_wndClass.lpszClassName = m_szClassName; // Name of this class
    m_wndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION); // Small icon for this class
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
    default:
        return DefWindowProc (hwnd, msg, wParam, lParam);
    }

    return 0;
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
