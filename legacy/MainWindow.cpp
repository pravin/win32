/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (March 08, 2007)
 * http://pravin.insanitybegins.com/win32
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>
#include <windowsx.h>
#define _WIN32_IE 0x0500 // To support INITCOMMONCONTROLSEX
#include <commctrl.h>
#include "MainWindow.h"
#include "AboutDialog.h"
#include "resource.h"

char MainWindow::m_szClassName[] = "DrawLite";
HINSTANCE MainWindow::m_hInstance = NULL;
HWND MainWindow::m_hStatusbar = NULL;
HWND MainWindow::m_hMainToolbar = NULL;
HWND MainWindow::m_hPaintToolbar = NULL;

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
    int width, height;
    switch (msg)
    {
    case WM_SIZE:
        // Resize the statusbar
		SendMessage(MainWindow::m_hStatusbar,msg,wParam,lParam);
        break;
    case WM_DESTROY:
        PostQuitMessage (0);
        break;
    case WM_COMMAND:
        HANDLE_WM_COMMAND(hwnd, wParam, lParam, OnCommand);
        break;
    case WM_CREATE:
        HANDLE_WM_CREATE(hwnd, wParam, lParam, OnCreate);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
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

    // Initialize Common controls
    INITCOMMONCONTROLSEX icx;
    // Ensure common control DLL is loaded
    icx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icx.dwICC = ICC_BAR_CLASSES; // Specify BAR classes
    InitCommonControlsEx(&icx); // Load the common control DLL

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

// Creates the toolbars and statusbar
// Parameters:
//  cs - Contains initialization parameters
// Returns:
//  void
bool MainWindow::OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
{
    const int numbuttons1 = 4;
    // Create Main Toolbar
    MainWindow::m_hMainToolbar = CreateWindowEx(
                                    0, TOOLBARCLASSNAME, NULL,
                                    WS_CHILD | TBSTYLE_FLAT,
                                    0, 0, 0, 0,
                                    hwnd, NULL, MainWindow::m_hInstance, NULL);

    HIMAGELIST hImageList1 = ImageList_Create(
                                16, 16,                 // 16x16 button size
                                ILC_COLOR16 | ILC_MASK, // ILC_MASK ensures transparent background
                                numbuttons1, 0); //
    // Set the image list.
    SendMessage(MainWindow::m_hMainToolbar, TB_SETIMAGELIST, (WPARAM)0,
        (LPARAM)hImageList1);

    // Load the button images.
    SendMessage(MainWindow::m_hMainToolbar, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR,
        (LPARAM)HINST_COMMCTRL);

    TBBUTTON tbButtons1[numbuttons1] = {
        {MAKELONG(STD_FILENEW, 0), IDM_FILE_NEW, TBSTATE_ENABLED,
            BTNS_AUTOSIZE, {0}, 0, 0},
        {MAKELONG(STD_FILEOPEN, 0), IDM_FILE_OPEN, TBSTATE_ENABLED,
            BTNS_AUTOSIZE, {0}, 0, 0},
        {MAKELONG(STD_FILESAVE, 0), IDM_FILE_SAVE, TBSTATE_ENABLED,
            BTNS_AUTOSIZE, {0}, 0, 0},
        {MAKELONG(STD_FILESAVE, 0), 0, TBSTATE_ENABLED,
            TBSTYLE_SEP, {0}, 0, 0}
    };

    // Add buttons
    SendMessage(MainWindow::m_hMainToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(MainWindow::m_hMainToolbar, TB_ADDBUTTONS, (WPARAM)numbuttons1, (LPARAM)&tbButtons1);

    // Show toolbar
    SendMessage(MainWindow::m_hMainToolbar, TB_AUTOSIZE, 0, 0);
    ShowWindow(MainWindow::m_hMainToolbar, TRUE);

    // Create Paint Toolbar

    // Create Statusbar
    MainWindow::m_hStatusbar = CreateStatusWindow(WS_CHILD|WS_VISIBLE, "Ready", hwnd, IDC_STATUSBAR);
    return true;
}
