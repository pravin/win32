/* DrawLite - Win32 Tutorial
 * Chapter 7 - Pixels you own
 *
 * File: mainwindow.c
 * The main application window.
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <strsafe.h>
#include "mainwindow.h"
#include "aboutdlg.h"
#include "newdlg.h"
#include "canvas.h"
#include "palette.h"
#include "resource.h"
#include "uiutil.h"

#define MAINWINDOW_CLASS L"DrawLite"

// Parts of the status bar
#define SB_PART_HINT    0
#define SB_PART_POS     1
#define SB_PART_TOOL    2

static LRESULT CALLBACK MainWindow_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL MainWindow_RegisterClass(HINSTANCE hInstance);
static BOOL MainWindow_OnCreate(MainWindow *mw);
static void MainWindow_OnCommand(MainWindow *mw, int id);
static void MainWindow_OnDpiChanged(MainWindow *mw, UINT dpi, const RECT *suggested);
static void MainWindow_Layout(MainWindow *mw);
static void MainWindow_SetTool(MainWindow *mw, ToolId tool);
static void MainWindow_ShowToolName(MainWindow *mw);
static HWND MainWindow_CreateToolbar(MainWindow *mw);
static void MainWindow_SetDocument(MainWindow *mw, Document *doc);
static void MainWindow_OnCanvasPos(MainWindow *mw, int x, int y);

MainWindow *
MainWindow_Create(HINSTANCE hInstance, int nCmdShow)
{
    MainWindow *mw;
    INITCOMMONCONTROLSEX icc;

    // The toolbar and status bar live in comctl32. Ask for them before using them.
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    if (!MainWindow_RegisterClass(hInstance))
        return NULL;

    mw = (MainWindow *)calloc(1, sizeof(MainWindow));
    if (!mw)
        return NULL;
    mw->hInstance = hInstance;

    mw->hwnd = CreateWindowExW(0, MAINWINDOW_CLASS, L"DrawLite",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 900, 640,
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
    if (!mw)
        return;
    if (mw->hFont)
        DeleteObject(mw->hFont);
    Doc_Destroy(mw->doc);
    free(mw);
}

HWND
MainWindow_GetHwnd(const MainWindow *mw)
{
    return mw->hwnd;
}

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
    // The grey of the "workspace" area. The canvas will sit on this later.
    wcx.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wcx.lpszMenuName = MAKEINTRESOURCEW(IDM_MAINMENU);
    wcx.lpszClassName = MAINWINDOW_CLASS;

    registered = RegisterClassExW(&wcx) != 0;
    return registered;
}

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
        break;
    case WM_CREATE:
        // Returning -1 from WM_CREATE stops the window from being created
        return MainWindow_OnCreate(mw) ? 0 : -1;
    case WM_SIZE:
        MainWindow_Layout(mw);
        return 0;
    case WM_DPICHANGED:
        // Sent when the window moves to a monitor with a different scale.
        MainWindow_OnDpiChanged(mw, HIWORD(wParam), (const RECT *)lParam);
        return 0;
    case WM_COMMAND:
        MainWindow_OnCommand(mw, LOWORD(wParam));
        return 0;
    case WMU_CANVAS_POS:
        MainWindow_OnCanvasPos(mw, (int)wParam, (int)lParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Function: MainWindow_OnCreate
// Creates the child windows: toolbar, tool palette and status bar.
static BOOL
MainWindow_OnCreate(MainWindow *mw)
{
    UINT dpi = GetDpiForWindow(mw->hwnd);

    mw->hFont = Ui_CreateFont(dpi);

    mw->hToolbar = MainWindow_CreateToolbar(mw);
    mw->hPalette = Palette_Create(mw->hwnd, mw->hInstance, IDC_PALETTE);
    mw->hCanvas = Canvas_Create(mw->hwnd, mw->hInstance, IDC_CANVAS);
    mw->hStatusbar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        mw->hwnd, (HMENU)(INT_PTR)IDC_STATUSBAR, mw->hInstance, NULL);
    if (!mw->hToolbar || !mw->hPalette || !mw->hCanvas || !mw->hStatusbar)
        return FALSE;

    Ui_SetFontOnChildren(mw->hwnd, mw->hFont);
    MainWindow_SetTool(mw, TOOL_PENCIL);

    // Start with a blank image so there is something to draw on
    mw->newWidth = 640;
    mw->newHeight = 480;
    MainWindow_SetDocument(mw, Doc_Create(mw->newWidth, mw->newHeight));
    return TRUE;
}

// Function: MainWindow_CreateToolbar
// Creates the toolbar with New, Open, Save, Undo and Redo buttons.
static HWND
MainWindow_CreateToolbar(MainWindow *mw)
{
    UINT dpi = GetDpiForWindow(mw->hwnd);
    HWND tb;
    TBBUTTON buttons[] =
    {
        { STD_FILENEW,  IDM_FILE_NEW,  TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"New" },
        { STD_FILEOPEN, IDM_FILE_OPEN, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Open" },
        { STD_FILESAVE, IDM_FILE_SAVE, TBSTATE_ENABLED, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Save" },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0 },
        { STD_UNDO,     IDM_EDIT_UNDO, 0, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Undo" },
        { STD_REDOW,    IDM_EDIT_REDO, 0, BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Redo" },
    };

    tb = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
        0, 0, 0, 0, mw->hwnd, (HMENU)(INT_PTR)IDC_TOOLBAR, mw->hInstance, NULL);
    if (!tb)
        return NULL;

    // Required before adding buttons, so the toolbar knows which TBBUTTON we use
    SendMessageW(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    // Windows ships a set of standard button pictures inside comctl32.
    // Pick the bigger set on high DPI screens.
    SendMessageW(tb, TB_LOADIMAGES, (dpi >= 144) ? IDB_STD_LARGE_COLOR : IDB_STD_SMALL_COLOR,
        (LPARAM)HINST_COMMCTRL);
    SendMessageW(tb, TB_ADDBUTTONSW, ARRAYSIZE(buttons), (LPARAM)buttons);
    return tb;
}

static void
MainWindow_OnCommand(MainWindow *mw, int id)
{
    if (id >= IDM_TOOL_FIRST && id < IDM_TOOL_FIRST + TOOL_COUNT)
    {
        MainWindow_SetTool(mw, (ToolId)(id - IDM_TOOL_FIRST));
        return;
    }

    switch (id)
    {
    case IDM_FILE_NEW:
        if (NewDlg_Show(mw->hInstance, mw->hwnd, &mw->newWidth, &mw->newHeight))
        {
            Document *doc = Doc_Create(mw->newWidth, mw->newHeight);
            if (doc)
                MainWindow_SetDocument(mw, doc);
            else
                MessageBoxW(mw->hwnd, L"Could not create an image that big.", L"DrawLite", MB_OK | MB_ICONERROR);
        }
        break;
    case IDM_FILE_OPEN:
    case IDM_FILE_SAVE:
    case IDM_EDIT_UNDO:
    case IDM_EDIT_REDO:
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

// Function: MainWindow_SetDocument
// Makes doc the current document and throws the old one away.
static void
MainWindow_SetDocument(MainWindow *mw, Document *doc)
{
    Document *old = mw->doc;
    wchar_t text[64];

    mw->doc = doc;
    Canvas_SetDocument(mw->hCanvas, doc);
    Doc_Destroy(old); // Only after the canvas has stopped looking at it

    if (doc)
    {
        StringCchPrintfW(text, ARRAYSIZE(text), L"Image: %d x %d", doc->width, doc->height);
        SendMessageW(mw->hStatusbar, SB_SETTEXTW, SB_PART_HINT, (LPARAM)text);
    }
}

// Function: MainWindow_OnCanvasPos
// Shows the mouse position (in image pixels) in the status bar.
static void
MainWindow_OnCanvasPos(MainWindow *mw, int x, int y)
{
    wchar_t text[64];

    StringCchPrintfW(text, ARRAYSIZE(text), L"%d, %d", x, y);
    SendMessageW(mw->hStatusbar, SB_SETTEXTW, SB_PART_POS, (LPARAM)text);
}

// Function: MainWindow_SetTool
// Selects a tool, and updates the palette and the status bar to match.
static void
MainWindow_SetTool(MainWindow *mw, ToolId tool)
{
    mw->tool = tool;
    Palette_SetTool(mw->hPalette, tool);
    MainWindow_ShowToolName(mw);
}

// Function: MainWindow_ShowToolName
// Writes the name of the current tool into the status bar.
static void
MainWindow_ShowToolName(MainWindow *mw)
{
    wchar_t text[64];

    StringCchPrintfW(text, ARRAYSIZE(text), L"Tool: %s", Tool_Name(mw->tool));
    SendMessageW(mw->hStatusbar, SB_SETTEXTW, SB_PART_TOOL, (LPARAM)text);
}

// Function: MainWindow_OnDpiChanged
// The window has moved to a screen with a different DPI. We get a new size
// from Windows, then rebuild the font and lay everything out again.
static void
MainWindow_OnDpiChanged(MainWindow *mw, UINT dpi, const RECT *suggested)
{
    HFONT oldFont = mw->hFont;

    mw->hFont = Ui_CreateFont(dpi);
    Ui_SetFontOnChildren(mw->hwnd, mw->hFont);
    if (oldFont)
        DeleteObject(oldFont);

    SetWindowPos(mw->hwnd, NULL, suggested->left, suggested->top,
        suggested->right - suggested->left, suggested->bottom - suggested->top,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

// Function: MainWindow_Layout
// Positions the child windows. Called whenever the window changes size.
//
//   +-----------------------------+
//   |           toolbar           |
//   +-------+---------------------+
//   |palette|       canvas        |
//   +-------+---------------------+
//   |          status bar         |
//   +-----------------------------+
static void
MainWindow_Layout(MainWindow *mw)
{
    RECT client, rcTool, rcStatus;
    UINT dpi = GetDpiForWindow(mw->hwnd);
    int toolbarHeight, statusHeight, paletteWidth, width;
    int parts[3];

    if (!mw->hToolbar || !mw->hStatusbar || !mw->hPalette || !mw->hCanvas)
        return;

    GetClientRect(mw->hwnd, &client);
    width = client.right;

    // Toolbar and status bar size and place themselves. We just have to nudge them.
    SendMessageW(mw->hToolbar, TB_AUTOSIZE, 0, 0);
    SendMessageW(mw->hStatusbar, WM_SIZE, 0, 0);
    GetWindowRect(mw->hToolbar, &rcTool);
    GetWindowRect(mw->hStatusbar, &rcStatus);
    toolbarHeight = rcTool.bottom - rcTool.top;
    statusHeight = rcStatus.bottom - rcStatus.top;

    // The status bar has three parts. Each number is where that part ends.
    parts[0] = max(width - Ui_Scale(dpi, 320), Ui_Scale(dpi, 100));
    parts[1] = max(width - Ui_Scale(dpi, 160), parts[0]);
    parts[2] = -1; // -1 means "all the way to the right"
    SendMessageW(mw->hStatusbar, SB_SETPARTS, ARRAYSIZE(parts), (LPARAM)parts);
    MainWindow_ShowToolName(mw); // Text is lost when the parts change, so set it again

    paletteWidth = Ui_Scale(dpi, 150);
    MoveWindow(mw->hPalette, 0, toolbarHeight, paletteWidth,
        client.bottom - toolbarHeight - statusHeight, TRUE);
    // The canvas gets whatever is left in the middle
    MoveWindow(mw->hCanvas, paletteWidth, toolbarHeight, width - paletteWidth,
        client.bottom - toolbarHeight - statusHeight, TRUE);
}
