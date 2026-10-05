/* DrawLite - Win32 Tutorial
 * Chapter 4 - Housekeeping
 *
 * File: mainwindow.c
 * The main application window.
 */

#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include "mainwindow.h"
#include "aboutdlg.h"
#include "resource.h"

#define MAINWINDOW_CLASS L"DrawLite"

static LRESULT CALLBACK MainWindow_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void MainWindow_OnCommand(MainWindow *mw, int id);
static BOOL MainWindow_RegisterClass(HINSTANCE hInstance);

MainWindow *
MainWindow_Create(HINSTANCE hInstance, int nCmdShow)
{
    MainWindow *mw;

    if (!MainWindow_RegisterClass(hInstance))
        return NULL;

    // calloc gives us zeroed memory, so every field starts as 0 or NULL
    mw = (MainWindow *)calloc(1, sizeof(MainWindow));
    if (!mw)
        return NULL;
    mw->hInstance = hInstance;

    // The last parameter is handed to our WndProc in WM_NCCREATE
    mw->hwnd = CreateWindowExW(0, MAINWINDOW_CLASS, L"DrawLite",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
        NULL, NULL, hInstance, mw);
    if (!mw->hwnd)
    {
        free(mw);
        return NULL;
    }

    ShowWindow(mw->hwnd, nCmdShow);
    UpdateWindow(mw->hwnd);
    return mw;
}

void
MainWindow_Destroy(MainWindow *mw)
{
    free(mw);
}

HWND
MainWindow_GetHwnd(const MainWindow *mw)
{
    return mw->hwnd;
}

// Function: MainWindow_RegisterClass
// Registers the window class. Registering twice fails, so we only do it once.
static BOOL
MainWindow_RegisterClass(HINSTANCE hInstance)
{
    static BOOL registered = FALSE;
    WNDCLASSEXW wcx;

    if (registered)
        return TRUE;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(WNDCLASSEXW);
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = MainWindow_WndProc;
    wcx.hInstance = hInstance;
    wcx.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
    wcx.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcx.lpszMenuName = MAKEINTRESOURCEW(IDM_MAINMENU);
    wcx.lpszClassName = MAINWINDOW_CLASS;

    registered = RegisterClassExW(&wcx) != 0;
    return registered;
}

// Function: MainWindow_WndProc
// The window procedure. Every message sent to our window passes through here.
// It finds our MainWindow struct again and hands over to the right function.
static LRESULT CALLBACK
MainWindow_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MainWindow *mw = (MainWindow *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        mw = (MainWindow *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)mw);
        mw->hwnd = hwnd;
        break; // let DefWindowProc finish creating the window
    case WM_COMMAND:
        MainWindow_OnCommand(mw, LOWORD(wParam));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Function: MainWindow_OnCommand
// Handles menu items and keyboard shortcuts.
//
// Parameters:
//   mw - The window the command was sent to
//   id - One of the IDM_ values from resource.h
static void
MainWindow_OnCommand(MainWindow *mw, int id)
{
    switch (id)
    {
    case IDM_FILE_NEW:
    case IDM_FILE_OPEN:
    case IDM_FILE_SAVE:
        MessageBoxW(mw->hwnd, L"Not written yet. Patience!", L"DrawLite", MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_FILE_EXIT:
        DestroyWindow(mw->hwnd);
        break;
    case IDM_HELP_ABOUT:
        AboutDlg_Show(mw->hInstance, mw->hwnd);
        break;
    }
}
