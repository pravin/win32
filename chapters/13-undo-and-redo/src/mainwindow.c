/* DrawLite - Win32 Tutorial
 * Chapter 13 - Undo and redo
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
#define SB_PART_ZOOM    2
#define SB_PART_TOOL    3

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
static void MainWindow_CheckRadio(MainWindow *mw, int first, int last, int checked);
static void MainWindow_OnCanvasPos(MainWindow *mw, int x, int y);
static void MainWindow_OnZoomChanged(MainWindow *mw, int percent);
static void MainWindow_UpdateUndoUI(MainWindow *mw);
static void MainWindow_Undo(MainWindow *mw, BOOL redo);

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
    case WMU_COLORS_CHANGED:
        Palette_Refresh(mw->hPalette);
        return 0;
    case WMU_CANVAS_POS:
        MainWindow_OnCanvasPos(mw, (int)wParam, (int)lParam);
        return 0;
    case WMU_DOC_CHANGED:
        MainWindow_UpdateUndoUI(mw);
        return 0;
    case WMU_ZOOM_CHANGED:
        MainWindow_OnZoomChanged(mw, (int)wParam);
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

    // Starting choices. Colours are normal 0xAARRGGBB values.
    mw->settings.tool = TOOL_PENCIL;
    mw->settings.primary = 0xFF000000;
    mw->settings.secondary = 0xFFFFFFFF;
    mw->settings.brushSize = 8;
    mw->settings.softBrush = FALSE;
    mw->settings.shapeStyle = SHAPE_OUTLINE;
    mw->settings.tolerance = 128;

    mw->hToolbar = MainWindow_CreateToolbar(mw);
    mw->hPalette = Palette_Create(mw->hwnd, mw->hInstance, IDC_PALETTE, &mw->settings);
    mw->hCanvas = Canvas_Create(mw->hwnd, mw->hInstance, IDC_CANVAS);
    mw->hStatusbar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        mw->hwnd, (HMENU)(INT_PTR)IDC_STATUSBAR, mw->hInstance, NULL);
    if (!mw->hToolbar || !mw->hPalette || !mw->hCanvas || !mw->hStatusbar)
        return FALSE;

    Ui_SetFontOnChildren(mw->hwnd, mw->hFont);
    MainWindow_SetTool(mw, TOOL_PENCIL);

    Canvas_SetSettings(mw->hCanvas, &mw->settings);
    MainWindow_CheckRadio(mw, IDM_BRUSH_FIRST, IDM_BRUSH_LAST, IDM_BRUSH_8);
    MainWindow_CheckRadio(mw, IDM_SHAPE_FIRST, IDM_SHAPE_LAST, IDM_SHAPE_OUTLINE);
    MainWindow_CheckRadio(mw, IDM_TOL_FIRST, IDM_TOL_LAST, IDM_TOL_50);

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

    if (id >= IDM_BRUSH_FIRST && id <= IDM_BRUSH_LAST)
    {
        static const int sizes[] = { 1, 2, 4, 8, 16, 32 };
        mw->settings.brushSize = sizes[id - IDM_BRUSH_FIRST];
        MainWindow_CheckRadio(mw, IDM_BRUSH_FIRST, IDM_BRUSH_LAST, id);
        return;
    }
    if (id >= IDM_TOL_FIRST && id <= IDM_TOL_LAST)
    {
        static const int tolerances[] = { 0, 25, 64, 128 }; // 0%, 10%, 25%, 50% of 255, roughly
        mw->settings.tolerance = tolerances[id - IDM_TOL_FIRST];
        MainWindow_CheckRadio(mw, IDM_TOL_FIRST, IDM_TOL_LAST, id);
        return;
    }
    if (id >= IDM_SHAPE_FIRST && id <= IDM_SHAPE_LAST)
    {
        mw->settings.shapeStyle = (ShapeStyle)(id - IDM_SHAPE_FIRST);
        MainWindow_CheckRadio(mw, IDM_SHAPE_FIRST, IDM_SHAPE_LAST, id);
        return;
    }

    switch (id)
    {
    case IDM_OPT_SOFT:
        mw->settings.softBrush = !mw->settings.softBrush;
        CheckMenuItem(GetMenu(mw->hwnd), IDM_OPT_SOFT, MF_BYCOMMAND | (mw->settings.softBrush ? MF_CHECKED : MF_UNCHECKED));
        break;
    case IDM_VIEW_ZOOMIN:
        Canvas_ZoomStep(mw->hCanvas, 1, -1, -1);
        break;
    case IDM_VIEW_ZOOMOUT:
        Canvas_ZoomStep(mw->hCanvas, -1, -1, -1);
        break;
    case IDM_VIEW_ACTUAL:
        Canvas_SetZoom(mw->hCanvas, 100, -1, -1);
        break;
    case IDM_VIEW_FIT:
        Canvas_ZoomToFit(mw->hCanvas);
        break;
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
    case IDM_EDIT_UNDO:
        MainWindow_Undo(mw, FALSE);
        break;
    case IDM_EDIT_REDO:
        MainWindow_Undo(mw, TRUE);
        break;
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

// Function: MainWindow_CheckRadio
// Ticks one menu item out of a run of ids and unticks the rest.
static void
MainWindow_CheckRadio(MainWindow *mw, int first, int last, int checked)
{
    HMENU menu = GetMenu(mw->hwnd);
    int i;

    for (i = first; i <= last; i++)
        CheckMenuItem(menu, i, MF_BYCOMMAND | (i == checked ? MF_CHECKED : MF_UNCHECKED));
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
    MainWindow_UpdateUndoUI(mw);
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

// Function: MainWindow_Undo
// Steps back (or forward, if redo is TRUE) through the history.
static void
MainWindow_Undo(MainWindow *mw, BOOL redo)
{
    RECT area;
    BOOL done;

    // The accelerator keys still work while the mouse is down. Not then, thanks.
    // (We ask the canvas, not GetCapture: a slider being dragged has the mouse captured too.)
    if (!mw->doc || Canvas_IsBusy(mw->hCanvas))
        return;

    if (redo)
        done = History_Redo(mw->doc->history, mw->doc->surface, &area);
    else
        done = History_Undo(mw->doc->history, mw->doc->surface, &area);

    if (done)
    {
        mw->doc->modified = TRUE;
        Canvas_InvalidateImageRect(mw->hCanvas, &area);
    }
    MainWindow_UpdateUndoUI(mw);
}

// Function: MainWindow_UpdateUndoUI
// Greys out Undo and Redo, in the menu and on the toolbar, when there is nothing to do.
static void
MainWindow_UpdateUndoUI(MainWindow *mw)
{
    BOOL canUndo = mw->doc && History_CanUndo(mw->doc->history);
    BOOL canRedo = mw->doc && History_CanRedo(mw->doc->history);
    HMENU menu = GetMenu(mw->hwnd);

    EnableMenuItem(menu, IDM_EDIT_UNDO, MF_BYCOMMAND | (canUndo ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, IDM_EDIT_REDO, MF_BYCOMMAND | (canRedo ? MF_ENABLED : MF_GRAYED));
    SendMessageW(mw->hToolbar, TB_ENABLEBUTTON, IDM_EDIT_UNDO, MAKELPARAM(canUndo, 0));
    SendMessageW(mw->hToolbar, TB_ENABLEBUTTON, IDM_EDIT_REDO, MAKELPARAM(canRedo, 0));
}

// Function: MainWindow_OnZoomChanged
// Shows the zoom level in the status bar.
static void
MainWindow_OnZoomChanged(MainWindow *mw, int percent)
{
    wchar_t text[32];

    StringCchPrintfW(text, ARRAYSIZE(text), L"%d%%", percent);
    SendMessageW(mw->hStatusbar, SB_SETTEXTW, SB_PART_ZOOM, (LPARAM)text);
}

// Function: MainWindow_SetTool
// Selects a tool, and updates the palette and the status bar to match.
static void
MainWindow_SetTool(MainWindow *mw, ToolId tool)
{
    mw->settings.tool = tool;
    Palette_SetTool(mw->hPalette, tool);
    MainWindow_ShowToolName(mw);
}

// Function: MainWindow_ShowToolName
// Writes the name of the current tool into the status bar.
static void
MainWindow_ShowToolName(MainWindow *mw)
{
    wchar_t text[64];

    StringCchPrintfW(text, ARRAYSIZE(text), L"Tool: %s", Tool_Name(mw->settings.tool));
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
    int parts[4];

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

    // The status bar has four parts. Each number is where that part ends.
    parts[0] = max(width - Ui_Scale(dpi, 380), Ui_Scale(dpi, 100));
    parts[1] = max(width - Ui_Scale(dpi, 250), parts[0]);
    parts[2] = max(width - Ui_Scale(dpi, 170), parts[1]);
    parts[3] = -1; // -1 means "all the way to the right"
    SendMessageW(mw->hStatusbar, SB_SETPARTS, ARRAYSIZE(parts), (LPARAM)parts);
    MainWindow_ShowToolName(mw); // Text is lost when the parts change, so set it again
    if (mw->doc)
        MainWindow_OnZoomChanged(mw, Canvas_GetZoom(mw->hCanvas));

    paletteWidth = Ui_Scale(dpi, 150);
    MoveWindow(mw->hPalette, 0, toolbarHeight, paletteWidth,
        client.bottom - toolbarHeight - statusHeight, TRUE);
    // The canvas gets whatever is left in the middle
    MoveWindow(mw->hCanvas, paletteWidth, toolbarHeight, width - paletteWidth,
        client.bottom - toolbarHeight - statusHeight, TRUE);
}
