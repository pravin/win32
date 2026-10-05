/* DrawLite - Win32 Tutorial
 * Chapter 2 - A window of your own
 * by Pravin Paratey (October 2002, rewritten 2026)
 *
 * Source released under the MIT licence. See LICENSE in the root folder.
 */

#include <windows.h>
#include <windowsx.h>
#include <strsafe.h>

// Everything this window needs to remember lives in here
typedef struct AppState
{
    int clicks; // How many times the left mouse button was pressed
} AppState;

// Callback function
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Windows entry point
int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    MSG msg;            // MSG structure to store messages
    WNDCLASSEXW wcx;    // Window class information
    HWND hwndMain;      // Main window handle
    AppState state = { 0 };

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize the struct to zero
    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(WNDCLASSEXW);               // Must always be sizeof(WNDCLASSEXW)
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // Class styles
    wcx.lpfnWndProc = MainWndProc;                  // Pointer to the callback procedure
    wcx.cbClsExtra = 0;                             // Extra bytes to allocate after the class
    wcx.cbWndExtra = 0;                             // Extra bytes to allocate after each window
    wcx.hInstance = hInstance;                      // Instance of the application
    wcx.hIcon = NULL;                               // Class icon
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);     // Class cursor
    wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Background brush
    wcx.lpszMenuName = NULL;                        // Menu resource
    wcx.lpszClassName = L"DrawLite";                // Name of this class
    wcx.hIconSm = NULL;                             // Small icon for this class

    // Register this window class with Windows
    if (!RegisterClassExW(&wcx))
        return 1;

    // Create the window
    hwndMain = CreateWindowExW(
        0,                                  // Extended window style
        L"DrawLite",                        // Window class name
        L"Chapter 2 - A window of your own",// Window title
        WS_OVERLAPPEDWINDOW,                // Window style
        CW_USEDEFAULT, CW_USEDEFAULT,       // (x,y) position of the window
        CW_USEDEFAULT, CW_USEDEFAULT,       // Width and height of the window
        NULL,                               // Parent window
        NULL,                               // Menu
        hInstance,                          // Application instance
        &state);                            // Pointer passed to WM_NCCREATE and WM_CREATE

    // Check if window creation was successful
    if (!hwndMain)
        return 1;

    // Make the window visible
    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    // Process messages coming to this window
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Return value to the system
    return (int)msg.wParam;
}

static LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Messages arrive before WM_NCCREATE too, so this can be NULL
    AppState *state = (AppState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        {
            // The pointer we gave CreateWindowExW comes along in the CREATESTRUCT.
            // Park it in the window so we can pick it up again later.
            CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    case WM_LBUTTONDOWN:
        {
            wchar_t str[100];
            state->clicks++;
            StringCchPrintfW(str, 100, L"Co-ordinates are\nX=%d and Y=%d\n\nClick number %d",
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), state->clicks);
            MessageBoxW(hwnd, str, L"Left Button Clicked", MB_OK);
        }
        break;
    case WM_DESTROY:
        // User closed the window
        PostQuitMessage(0);
        break;
    default:
        // Call the default window handler
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}
