/* DrawLite - Win32 Tutorial
 * Chapter 3 - Resources: Icons, Menus and Dialogs
 * by Pravin Paratey (May 2007, rewritten 2026)
 *
 * Source released under the MIT licence. See LICENSE in the root folder.
 */

#include <windows.h>
#include <windowsx.h>
#include "resource.h"

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

static HINSTANCE g_hInstance; // We need this again when we show the About box

int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    WNDCLASSEXW wcx;
    HWND hwndMain;
    HACCEL hAccel;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_hInstance = hInstance;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(WNDCLASSEXW);
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = MainWndProc;
    wcx.hInstance = hInstance;
    // Icons and the menu come from the resources linked into our exe
    wcx.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
    wcx.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcx.lpszMenuName = MAKEINTRESOURCEW(IDM_MAINMENU);
    wcx.lpszClassName = L"DrawLite";

    if (!RegisterClassExW(&wcx))
        return 1;

    hwndMain = CreateWindowExW(0, L"DrawLite", L"DrawLite",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
        NULL, NULL, hInstance, NULL);
    if (!hwndMain)
        return 1;

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    // Load the keyboard shortcuts
    hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDA_MAINACCEL));

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        // Give the accelerator table first go at the message.
        // If it was a shortcut, it turns into a WM_COMMAND and we are done.
        if (!TranslateAcceleratorW(hwndMain, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return (int)msg.wParam;
}

static LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        // Menu items and accelerators both land here. The id is in the low word of wParam.
        switch (LOWORD(wParam))
        {
        case IDM_FILE_NEW:
        case IDM_FILE_OPEN:
        case IDM_FILE_SAVE:
            MessageBoxW(hwnd, L"Not written yet. Patience!", L"DrawLite", MB_OK | MB_ICONINFORMATION);
            break;
        case IDM_FILE_EXIT:
            // Ask the window to close. This also sends WM_DESTROY.
            DestroyWindow(hwnd);
            break;
        case IDM_HELP_ABOUT:
            DialogBoxParamW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUT), hwnd, AboutDlgProc, 0);
            break;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Function: AboutDlgProc
// Handles messages for the About box. Dialog procs return TRUE if they
// handled a message and FALSE if they did not.
static INT_PTR CALLBACK
AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
