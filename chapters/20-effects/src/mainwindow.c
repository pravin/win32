/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: mainwindow.c
 * The main application window.
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdlib.h>
#include <strsafe.h>
#include "mainwindow.h"
#include "aboutdlg.h"
#include "newdlg.h"
#include "imageio.h"
#include "clipboard.h"
#include "composite.h"
#include "effects.h"
#include "dlt.h"
#include "layerpanel.h"
#include "namedlg.h"
#include "selops.h"
#include "history.h"
#include "filedlg.h"
#include <shellapi.h>
#include <string.h>
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
static void MainWindow_ApplyEffect(MainWindow *mw, int id);
static void MainWindow_Undo(MainWindow *mw, BOOL redo);
static void MainWindow_UpdateTitle(MainWindow *mw);
static BOOL MainWindow_ConfirmDiscard(MainWindow *mw);
static BOOL MainWindow_Save(MainWindow *mw, BOOL saveAs);
static void MainWindow_OpenFile(MainWindow *mw, const wchar_t *path);
static void MainWindow_NewImage(MainWindow *mw);
static void MainWindow_PasteImage(MainWindow *mw);
static void MainWindow_PasteLayer(MainWindow *mw);
static void MainWindow_LayerCommand(MainWindow *mw, int id);
static void MainWindow_UpdateImageInfo(MainWindow *mw);
static void MainWindow_DocChanged(MainWindow *mw);
static void MainWindow_RepaintAll(MainWindow *mw);
static void MainWindow_OnLayerMessage(MainWindow *mw, UINT msg, WPARAM wParam, LPARAM lParam);
static void MainWindow_PushProps(MainWindow *mw, int index, const Layer *before);
static void MainWindow_SelectCommand(MainWindow *mw, int id);
static void MainWindow_Copy(MainWindow *mw, BOOL cut);

MainWindow *
MainWindow_Create(HINSTANCE hInstance, int nCmdShow, const wchar_t *cmdLine)
{
    MainWindow *mw;
    INITCOMMONCONTROLSEX icc;

    // The toolbar and status bar live in comctl32. Ask for them before using them.
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS;
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

    // A file name on the command line, perhaps in quotes, gets opened
    if (cmdLine && cmdLine[0])
    {
        wchar_t path[MAX_PATH];
        size_t len;

        StringCchCopyW(path, ARRAYSIZE(path), cmdLine[0] == L'"' ? cmdLine + 1 : cmdLine);
        len = wcslen(path);
        if (len > 0 && path[len - 1] == L'"')
            path[len - 1] = L'\0';
        MainWindow_OpenFile(mw, path);
    }
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
    case WM_CLOSE:
        // Give the user the chance to save first
        if (MainWindow_ConfirmDiscard(mw))
            DestroyWindow(hwnd);
        return 0;
    case WM_DROPFILES:
    {
        // A file was dropped on the window. We only open the first one.
        wchar_t path[MAX_PATH];
        if (DragQueryFileW((HDROP)wParam, 0, path, ARRAYSIZE(path)))
            MainWindow_OpenFile(mw, path);
        DragFinish((HDROP)wParam);
        return 0;
    }
    case WM_COMMAND:
        MainWindow_OnCommand(mw, LOWORD(wParam));
        return 0;
    case WMU_COLORS_CHANGED:
        Palette_Refresh(mw->hPalette);
        return 0;
    case WMU_CANVAS_POS:
        MainWindow_OnCanvasPos(mw, (int)wParam, (int)lParam);
        return 0;
    case WMU_LAYER_SELECT:
    case WMU_LAYER_VISIBLE:
    case WMU_LAYER_OPACITY:
    case WMU_LAYER_BLEND:
    case WMU_LAYER_RENAME:
        MainWindow_OnLayerMessage(mw, msg, wParam, lParam);
        return 0;
    case WMU_DOC_CHANGED:
        MainWindow_DocChanged(mw);
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

    // Text starts out as 24 pixel Segoe UI. The height is in IMAGE pixels, so it does
    // not change with the screen's DPI: a picture is the same picture on every monitor.
    mw->settings.font.lfHeight = -24;
    mw->settings.font.lfWeight = FW_NORMAL;
    mw->settings.font.lfCharSet = DEFAULT_CHARSET;
    mw->settings.font.lfQuality = ANTIALIASED_QUALITY;
    StringCchCopyW(mw->settings.font.lfFaceName, LF_FACESIZE, L"Segoe UI");

    mw->hToolbar = MainWindow_CreateToolbar(mw);
    mw->hPalette = Palette_Create(mw->hwnd, mw->hInstance, IDC_PALETTE, &mw->settings);
    mw->hCanvas = Canvas_Create(mw->hwnd, mw->hInstance, IDC_CANVAS);
    mw->hLayers = LayerPanel_Create(mw->hwnd, mw->hInstance, IDC_LAYERS);
    mw->hStatusbar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        mw->hwnd, (HMENU)(INT_PTR)IDC_STATUSBAR, mw->hInstance, NULL);
    if (!mw->hToolbar || !mw->hPalette || !mw->hCanvas || !mw->hLayers || !mw->hStatusbar)
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
    DragAcceptFiles(mw->hwnd, TRUE);   // Let people drop image files on the window
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

// Function: MainWindow_ApplyEffect
// Runs one of the Effects menu items on the active layer.
static void
MainWindow_ApplyEffect(MainWindow *mw, int id)
{
    EffectId fx;
    int param = 0;
    RECT area;

    if (!mw->doc || Canvas_IsBusy(mw->hCanvas))
        return;
    switch (id)
    {
    case IDM_FX_INVERT:         fx = FX_INVERT; break;
    case IDM_FX_GRAYSCALE:      fx = FX_GRAYSCALE; break;
    case IDM_FX_SEPIA:          fx = FX_SEPIA; break;
    case IDM_FX_BRIGHTER:       fx = FX_BRIGHTEN; param = 40; break;
    case IDM_FX_DARKER:         fx = FX_BRIGHTEN; param = -40; break;
    case IDM_FX_BLUR_SMALL:     fx = FX_BLUR; param = 2; break;
    case IDM_FX_BLUR_MEDIUM:    fx = FX_BLUR; param = 6; break;
    case IDM_FX_BLUR_LARGE:     fx = FX_BLUR; param = 16; break;
    case IDM_FX_PIXELATE:       fx = FX_PIXELATE; param = 8; break;
    default:                    return;
    }

    // Effects can take a while. Show the hourglass for the short ones.
    SetCursor(LoadCursorW(NULL, IDC_WAIT));
    if (Effect_Run(mw->hInstance, mw->hwnd, mw->doc, fx, param, &area))
    {
        Canvas_InvalidateImageRect(mw->hCanvas, &area);
        MainWindow_DocChanged(mw);
    }
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

static void
MainWindow_OnCommand(MainWindow *mw, int id)
{
    // Any command finishes the text being typed, so it is in the picture before we act
    Canvas_CommitText(mw->hCanvas);

    if (id >= IDM_FX_FIRST && id <= IDM_FX_LAST)
    {
        MainWindow_ApplyEffect(mw, id);
        return;
    }

    if (id == IDM_OPT_FONT)
    {
        CHOOSEFONTW cf;
        LOGFONTW lf = mw->settings.font;
        UINT dpi = GetDpiForWindow(mw->hwnd);

        // The dialog thinks in screen pixels at the current DPI, we think in image pixels.
        lf.lfHeight = MulDiv(lf.lfHeight, (int)dpi, 96);
        ZeroMemory(&cf, sizeof(cf));
        cf.lStructSize = sizeof(cf);
        cf.hwndOwner = mw->hwnd;
        cf.lpLogFont = &lf;
        cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOVERTFONTS | CF_NOSCRIPTSEL;
        if (ChooseFontW(&cf))
        {
            lf.lfHeight = MulDiv(lf.lfHeight, 96, (int)dpi);
            lf.lfQuality = ANTIALIASED_QUALITY;
            mw->settings.font = lf;
        }
        return;
    }

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

    if (id >= IDM_LAYER_ADD && id <= IDM_LAYER_PREV)
    {
        MainWindow_LayerCommand(mw, id);
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
        MainWindow_NewImage(mw);
        break;
    case IDM_EDIT_COPY:
        MainWindow_Copy(mw, FALSE);
        break;
    case IDM_EDIT_CUT:
        MainWindow_Copy(mw, TRUE);
        break;
    case IDM_EDIT_DELETE:
        if (mw->doc && !Canvas_IsBusy(mw->hCanvas) && SelOps_ClearPixels(mw->doc))
        {
            MainWindow_RepaintAll(mw);
            MainWindow_DocChanged(mw);
        }
        break;
    case IDM_SEL_ALL:
    case IDM_SEL_NONE:
    case IDM_SEL_INVERT:
        MainWindow_SelectCommand(mw, id);
        break;
    case IDM_EDIT_PASTE:
        MainWindow_PasteLayer(mw);
        break;
    case IDM_EDIT_PASTE_IMAGE:
        MainWindow_PasteImage(mw);
        break;
    case IDM_EDIT_UNDO:
        MainWindow_Undo(mw, FALSE);
        break;
    case IDM_EDIT_REDO:
        MainWindow_Undo(mw, TRUE);
        break;
    case IDM_FILE_OPEN:
    {
        wchar_t path[MAX_PATH];
        if (FileDlg_Open(mw->hwnd, path))
            MainWindow_OpenFile(mw, path);
        break;
    }
    case IDM_FILE_SAVE:
        MainWindow_Save(mw, FALSE);
        break;
    case IDM_FILE_SAVEAS:
        MainWindow_Save(mw, TRUE);
        break;
    case IDM_FILE_EXIT:
        // Go through WM_CLOSE, so the "save changes?" question is asked
        SendMessageW(mw->hwnd, WM_CLOSE, 0, 0);
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

    mw->doc = doc;
    Canvas_SetDocument(mw->hCanvas, doc);
    LayerPanel_SetDocument(mw->hLayers, doc);
    Doc_Destroy(old); // Only after the canvas has stopped looking at it

    MainWindow_UpdateImageInfo(mw);
    MainWindow_UpdateUndoUI(mw);
    MainWindow_UpdateTitle(mw);
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

// Function: MainWindow_FileName
// Returns the part of a path after the last slash. For "" returns "Untitled".
static const wchar_t *
MainWindow_FileName(const wchar_t *path)
{
    const wchar_t *name = path, *p;

    if (!path[0])
        return L"Untitled";
    for (p = path; *p; p++)
        if (*p == L'\\' || *p == L'/')
            name = p + 1;
    return name;
}

// Function: MainWindow_UpdateTitle
// Puts "name* - DrawLite" in the title bar. The star means unsaved changes.
static void
MainWindow_UpdateTitle(MainWindow *mw)
{
    wchar_t title[MAX_PATH + 32];

    if (mw->doc)
        StringCchPrintfW(title, ARRAYSIZE(title), L"%s%s - DrawLite",
            MainWindow_FileName(mw->doc->path), mw->doc->modified ? L"*" : L"");
    else
        StringCchCopyW(title, ARRAYSIZE(title), L"DrawLite");
    SetWindowTextW(mw->hwnd, title);
}

// Function: MainWindow_ConfirmDiscard
// If the image has unsaved changes, asks the user what to do about them.
//
// Returns:
//   TRUE if it is fine to throw the image away (nothing to save, the user
//   chose No, or the user chose Yes and it was saved). FALSE if the user
//   cancelled, or saving failed, and we should stay where we are.
static BOOL
MainWindow_ConfirmDiscard(MainWindow *mw)
{
    wchar_t text[MAX_PATH + 64];

    if (!mw->doc || !mw->doc->modified)
        return TRUE;

    StringCchPrintfW(text, ARRAYSIZE(text), L"Save changes to %s?", MainWindow_FileName(mw->doc->path));
    switch (MessageBoxW(mw->hwnd, text, L"DrawLite", MB_YESNOCANCEL | MB_ICONQUESTION))
    {
    case IDYES:
        return MainWindow_Save(mw, FALSE);
    case IDNO:
        return TRUE;
    default:
        return FALSE;
    }
}

// Function: MainWindow_Save
// Saves the image. Asks for a file name if there isn't one yet, or if saveAs is TRUE.
//
// Returns:
//   TRUE if the image was saved.
static BOOL
MainWindow_Save(MainWindow *mw, BOOL saveAs)
{
    wchar_t path[MAX_PATH];
    wchar_t error[128];

    if (!mw->doc)
        return FALSE;

    StringCchCopyW(path, ARRAYSIZE(path), mw->doc->path);
    // Also ask if we have a name but cannot save in that format (a .gif we opened, say)
    if (saveAs || !path[0] || !(ImageIO_CanSave(path) || Dlt_IsDltPath(path)))
    {
        if (!FileDlg_SaveAs(mw->hwnd, path))
            return FALSE;
    }

    if (Dlt_IsDltPath(path))
    {
        if (!Dlt_Save(mw->doc, path, error, ARRAYSIZE(error)))
        {
            MessageBoxW(mw->hwnd, error, L"DrawLite", MB_OK | MB_ICONERROR);
            return FALSE;
        }
    }
    else
    {
        // Every other format knows nothing about layers, so we save them
        // squashed together. Say so, once, so nobody loses work by surprise.
        if (mw->doc->layerCount > 1 && !mw->doc->warnedFlat)
        {
            if (MessageBoxW(mw->hwnd,
                    L"This file format cannot hold layers. DrawLite will save them merged into one.\n\n"
                    L"Your layers stay as they are in the editor. To keep them in a file, save as a DrawLite (.dlt) file.",
                    L"DrawLite", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
                return FALSE;
            mw->doc->warnedFlat = TRUE;
        }
        {
            Surface *flat = Doc_Flatten(mw->doc);
            BOOL ok = flat && ImageIO_Save(flat, path, error, ARRAYSIZE(error));
            if (!flat)
                StringCchCopyW(error, ARRAYSIZE(error), L"Not enough memory to save.");
            Surface_Destroy(flat);
            if (!ok)
            {
                MessageBoxW(mw->hwnd, error, L"DrawLite", MB_OK | MB_ICONERROR);
                return FALSE;
            }
        }
    }
    StringCchCopyW(mw->doc->path, ARRAYSIZE(mw->doc->path), path);
    mw->doc->modified = FALSE;
    MainWindow_UpdateTitle(mw);
    return TRUE;
}

// Function: MainWindow_OpenFile
// Loads an image from disk, replacing the current one.
static void
MainWindow_OpenFile(MainWindow *mw, const wchar_t *path)
{
    wchar_t error[128];
    Surface *surface;
    Document *doc;

    if (!MainWindow_ConfirmDiscard(mw))
        return;

    if (Dlt_IsDltPath(path))
    {
        // Our own format. It comes back with all its layers.
        doc = Dlt_Load(path, error, ARRAYSIZE(error));
        if (!doc)
        {
            MessageBoxW(mw->hwnd, error, L"DrawLite", MB_OK | MB_ICONERROR);
            return;
        }
    }
    else
    {
        surface = ImageIO_Load(path, error, ARRAYSIZE(error));
        if (!surface)
        {
            MessageBoxW(mw->hwnd, error, L"DrawLite", MB_OK | MB_ICONERROR);
            return;
        }
        doc = Doc_CreateFromSurface(surface);
        if (!doc)
        {
            Surface_Destroy(surface);
            MessageBoxW(mw->hwnd, L"Not enough memory.", L"DrawLite", MB_OK | MB_ICONERROR);
            return;
        }
    }
    StringCchCopyW(doc->path, ARRAYSIZE(doc->path), path);
    MainWindow_SetDocument(mw, doc);
}

// Function: MainWindow_UpdateImageInfo
// Shows the image size and the active layer in the left part of the status bar.
static void
MainWindow_UpdateImageInfo(MainWindow *mw)
{
    wchar_t text[128];

    if (!mw->doc)
        return;
    StringCchPrintfW(text, ARRAYSIZE(text), L"%d x %d   Layer %d of %d: %s%s",
        mw->doc->width, mw->doc->height, mw->doc->active + 1, mw->doc->layerCount,
        Doc_ActiveLayer(mw->doc)->name, Doc_ActiveLayer(mw->doc)->visible ? L"" : L" (hidden)");
    SendMessageW(mw->hStatusbar, SB_SETTEXTW, SB_PART_HINT, (LPARAM)text);
}

// Function: MainWindow_DocChanged
// Brings everything that shows document state up to date.
static void
MainWindow_DocChanged(MainWindow *mw)
{
    MainWindow_UpdateImageInfo(mw);
    MainWindow_UpdateUndoUI(mw);
    MainWindow_UpdateTitle(mw);
    LayerPanel_Refresh(mw->hLayers);
}

// Function: MainWindow_Copy
// Copies to the clipboard. With a selection, that is the selected part of the
// active layer. Without, it is the whole picture as you see it. Cut also deletes.
static void
MainWindow_Copy(MainWindow *mw, BOOL cut)
{
    Surface *copy;
    BOOL ok;

    if (!mw->doc || Canvas_IsBusy(mw->hCanvas))
        return;
    if (Sel_IsActive(&mw->doc->sel))
        copy = SelOps_CopyPixels(mw->doc);
    else
    {
        if (cut)
            return;     // Nothing to cut
        copy = Doc_Flatten(mw->doc);
    }
    ok = copy && Clipboard_CopyImage(mw->hwnd, copy);
    Surface_Destroy(copy);
    if (!ok)
    {
        MessageBoxW(mw->hwnd, L"The picture could not be copied.", L"DrawLite", MB_OK | MB_ICONERROR);
        return;
    }
    if (cut && SelOps_ClearPixels(mw->doc))
    {
        MainWindow_RepaintAll(mw);
        MainWindow_DocChanged(mw);
    }
}

// Function: MainWindow_SelectCommand
// Select All, Deselect and Invert. Each one goes into the history.
static void
MainWindow_SelectCommand(MainWindow *mw, int id)
{
    Document *doc = mw->doc;
    uint8_t *before, *after;
    BOOL changed;

    if (!doc || Canvas_IsBusy(mw->hCanvas))
        return;
    before = Sel_CloneMask(&doc->sel, doc->width, doc->height);
    switch (id)
    {
    case IDM_SEL_ALL:
        Sel_SelectAll(&doc->sel, doc->width, doc->height);
        break;
    case IDM_SEL_NONE:
        Sel_Clear(&doc->sel);
        break;
    case IDM_SEL_INVERT:
        Sel_Invert(&doc->sel, doc->width, doc->height);
        break;
    }
    after = Sel_CloneMask(&doc->sel, doc->width, doc->height);

    changed = (before != NULL) != (after != NULL)
        || (before && after && memcmp(before, after, (size_t)doc->width * doc->height) != 0);
    if (changed)
        History_PushSelection(doc->history, before, after, doc->width, doc->height);
    else
    {
        free(before);
        free(after);
    }
    MainWindow_RepaintAll(mw);
    MainWindow_DocChanged(mw);
}

// Function: MainWindow_PushProps
// Records a change to a layer's settings, if there was one. "before" is a copy
// of the layer as it was.
static void
MainWindow_PushProps(MainWindow *mw, int index, const Layer *before)
{
    const Layer *now = mw->doc->layers[index];

    if (before->visible == now->visible && before->opacity == now->opacity
        && before->blend == now->blend && wcscmp(before->name, now->name) == 0)
        return;     // Nothing changed
    History_PushLayerProps(mw->doc->history, index, before, now);
    mw->doc->modified = TRUE;
}

// Function: MainWindow_OnLayerMessage
// The layers panel tells us what the user did. We change the document.
static void
MainWindow_OnLayerMessage(MainWindow *mw, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Document *doc = mw->doc;
    int index = (int)wParam;
    Layer before;

    if (!doc || Canvas_IsBusy(mw->hCanvas))
        return;

    switch (msg)
    {
    case WMU_LAYER_SELECT:
        if (index >= 0 && index < doc->layerCount)
            doc->active = index;
        MainWindow_UpdateImageInfo(mw);
        LayerPanel_Refresh(mw->hLayers);
        return;
    case WMU_LAYER_VISIBLE:
        if (index < 0 || index >= doc->layerCount)
            return;
        Layer_CopyProps(&before, doc->layers[index]);
        doc->layers[index]->visible = !doc->layers[index]->visible;
        MainWindow_PushProps(mw, index, &before);
        break;
    case WMU_LAYER_OPACITY:
        // Dragging the slider sends lots of messages. Remember how the layer
        // was at the start, show every step live, and only record one undo
        // step, at the end.
        if (!mw->opacityDragging)
        {
            Layer_CopyProps(&mw->opacityBefore, Doc_ActiveLayer(doc));
            mw->opacityDragging = TRUE;
        }
        Doc_ActiveLayer(doc)->opacity = max(0, min(255, (int)wParam));
        if (lParam)     // The user has let go
        {
            mw->opacityDragging = FALSE;
            MainWindow_PushProps(mw, doc->active, &mw->opacityBefore);
            break;
        }
        Doc_UpdateAllOfView(doc);
        MainWindow_RepaintAll(mw);
        return;
    case WMU_LAYER_BLEND:
        if (wParam >= BLEND_COUNT)
            return;
        Layer_CopyProps(&before, Doc_ActiveLayer(doc));
        Doc_ActiveLayer(doc)->blend = (BlendMode)wParam;
        MainWindow_PushProps(mw, doc->active, &before);
        break;
    case WMU_LAYER_RENAME:
    {
        wchar_t name[LAYER_NAME_MAX];

        if (index < 0 || index >= doc->layerCount)
            return;
        Layer_CopyProps(&before, doc->layers[index]);
        StringCchCopyW(name, LAYER_NAME_MAX, before.name);
        if (!NameDlg_Show(mw->hInstance, mw->hwnd, name, LAYER_NAME_MAX))
            return;
        StringCchCopyW(doc->layers[index]->name, LAYER_NAME_MAX, name);
        MainWindow_PushProps(mw, index, &before);
        break;
    }
    }
    Doc_UpdateAllOfView(doc);
    MainWindow_RepaintAll(mw);
    MainWindow_DocChanged(mw);
}

// Function: MainWindow_RepaintAll
// Repaints the whole picture.
static void
MainWindow_RepaintAll(MainWindow *mw)
{
    RECT all;
    SetRect(&all, 0, 0, mw->doc->width, mw->doc->height);
    Canvas_InvalidateImageRect(mw->hCanvas, &all);
}

// Function: MainWindow_LayerCommand
// All the items on the Layer menu. Every command that changes the stack
// writes what it did into the history, so that undo can reverse it.
static void
MainWindow_LayerCommand(MainWindow *mw, int id)
{
    Document *doc = mw->doc;
    wchar_t name[LAYER_NAME_MAX];
    Layer *layer;
    int at;

    // Don't change the stack in the middle of a brush stroke
    if (!doc || Canvas_IsBusy(mw->hCanvas))
        return;

    switch (id)
    {
    case IDM_LAYER_ADD:
        StringCchPrintfW(name, ARRAYSIZE(name), L"Layer %d", doc->layerCount + 1);
        layer = Layer_Create(doc->width, doc->height, name);
        if (!layer)
            return;
        at = doc->active + 1;               // Just above the active layer
        if (!Doc_InsertLayer(doc, at, layer))
        {
            Layer_Destroy(layer);
            return;
        }
        History_PushLayerAdded(doc->history, at);
        break;
    case IDM_LAYER_DUPLICATE:
        layer = Layer_Clone(Doc_ActiveLayer(doc));
        if (!layer)
            return;
        StringCchCatW(layer->name, LAYER_NAME_MAX, L" copy");
        at = doc->active + 1;
        if (!Doc_InsertLayer(doc, at, layer))
        {
            Layer_Destroy(layer);
            return;
        }
        History_PushLayerAdded(doc->history, at);
        break;
    case IDM_LAYER_DELETE:
        if (doc->layerCount < 2)
        {
            MessageBoxW(mw->hwnd, L"An image needs at least one layer.", L"DrawLite", MB_OK | MB_ICONINFORMATION);
            return;
        }
        at = doc->active;
        layer = Doc_RemoveLayer(doc, at);
        // The history keeps the layer now, in case the user wants it back
        History_PushLayerRemoved(doc->history, layer, at);
        break;
    case IDM_LAYER_UP:
    case IDM_LAYER_DOWN:
    {
        int to = doc->active + (id == IDM_LAYER_UP ? 1 : -1);
        if (to < 0 || to >= doc->layerCount)
            return;
        at = doc->active;
        Doc_MoveLayer(doc, at, to);
        History_PushLayerMoved(doc->history, at, to);
        break;
    }
    case IDM_LAYER_VISIBLE:
    {
        Layer before;
        Layer *active = Doc_ActiveLayer(doc);

        Layer_CopyProps(&before, active);
        active->visible = !active->visible;
        MainWindow_PushProps(mw, doc->active, &before);
        Doc_UpdateAllOfView(doc);
        break;
    }
    case IDM_LAYER_NEXT:
    case IDM_LAYER_PREV:
    {
        int to = doc->active + (id == IDM_LAYER_NEXT ? 1 : -1);
        if (to >= 0 && to < doc->layerCount)
            doc->active = to;
        MainWindow_UpdateImageInfo(mw);
        return;     // Choosing a layer is not a change to the picture
    }
    default:
        return;
    }

    doc->modified = TRUE;
    MainWindow_RepaintAll(mw);
    MainWindow_DocChanged(mw);
}

// Function: MainWindow_PasteLayer
// Puts the picture from the clipboard on a new layer, at the top left.
static void
MainWindow_PasteLayer(MainWindow *mw)
{
    wchar_t error[128] = L"";
    Document *doc = mw->doc;
    Surface *pasted;
    Layer *layer;
    int w, h, y, at;

    if (!doc || Canvas_IsBusy(mw->hCanvas))
        return;
    if (!Clipboard_HasImage())
    {
        MessageBoxW(mw->hwnd, L"There is no picture on the clipboard.", L"DrawLite", MB_OK | MB_ICONINFORMATION);
        return;
    }
    pasted = Clipboard_PasteImage(mw->hwnd, error, ARRAYSIZE(error));
    if (!pasted)
    {
        MessageBoxW(mw->hwnd, error, L"DrawLite", MB_OK | MB_ICONERROR);
        return;
    }
    layer = Layer_Create(doc->width, doc->height, L"Pasted");
    if (!layer)
    {
        Surface_Destroy(pasted);
        return;
    }
    // A layer is always as big as the image. Copy as much as fits.
    w = min(pasted->width, doc->width);
    h = min(pasted->height, doc->height);
    for (y = 0; y < h; y++)
        memcpy(layer->surface->pixels + (size_t)y * doc->width,
            pasted->pixels + (size_t)y * pasted->width, (size_t)w * sizeof(uint32_t));
    Surface_Destroy(pasted);

    at = doc->active + 1;
    if (!Doc_InsertLayer(doc, at, layer))
    {
        Layer_Destroy(layer);
        return;
    }
    History_PushLayerAdded(doc->history, at);
    doc->modified = TRUE;
    MainWindow_RepaintAll(mw);
    MainWindow_DocChanged(mw);
}

// Function: MainWindow_PasteImage
// Makes a new image out of whatever picture is on the clipboard.
static void
MainWindow_PasteImage(MainWindow *mw)
{
    wchar_t error[128] = L"";
    Surface *surface;
    Document *doc;

    if (!Clipboard_HasImage())
    {
        MessageBoxW(mw->hwnd, L"There is no picture on the clipboard.", L"DrawLite", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!MainWindow_ConfirmDiscard(mw))
        return;

    surface = Clipboard_PasteImage(mw->hwnd, error, ARRAYSIZE(error));
    if (!surface)
    {
        MessageBoxW(mw->hwnd, error, L"DrawLite", MB_OK | MB_ICONERROR);
        return;
    }
    doc = Doc_CreateFromSurface(surface);
    if (!doc)
    {
        Surface_Destroy(surface);
        return;
    }
    doc->modified = TRUE;       // It has never been saved, so it has something to lose
    MainWindow_SetDocument(mw, doc);
}

// Function: MainWindow_NewImage
// Asks for a size, then starts with a blank white image.
static void
MainWindow_NewImage(MainWindow *mw)
{
    Document *doc;

    if (!MainWindow_ConfirmDiscard(mw))
        return;
    if (!NewDlg_Show(mw->hInstance, mw->hwnd, &mw->newWidth, &mw->newHeight))
        return;

    doc = Doc_Create(mw->newWidth, mw->newHeight);
    if (doc)
        MainWindow_SetDocument(mw, doc);
    else
        MessageBoxW(mw->hwnd, L"Could not create an image that big.", L"DrawLite", MB_OK | MB_ICONERROR);
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
        done = History_Redo(mw->doc->history, mw->doc, &area);
    else
        done = History_Undo(mw->doc->history, mw->doc, &area);

    if (done)
    {
        mw->doc->modified = TRUE;
        Canvas_InvalidateImageRect(mw->hCanvas, &area);
    }
    MainWindow_DocChanged(mw);
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
//   +-------+-----------------+---+
//   |palette|     canvas      |lay|
//   +-------+-----------------+---+
//   |          status bar         |
//   +-----------------------------+
static void
MainWindow_Layout(MainWindow *mw)
{
    RECT client, rcTool, rcStatus;
    UINT dpi = GetDpiForWindow(mw->hwnd);
    int toolbarHeight, statusHeight, paletteWidth, layersWidth, width;
    int parts[4];

    if (!mw->hToolbar || !mw->hStatusbar || !mw->hPalette || !mw->hCanvas || !mw->hLayers)
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
    layersWidth = Ui_Scale(dpi, 200);
    MoveWindow(mw->hLayers, width - layersWidth, toolbarHeight, layersWidth,
        client.bottom - toolbarHeight - statusHeight, TRUE);
    // The canvas gets whatever is left in the middle
    MoveWindow(mw->hCanvas, paletteWidth, toolbarHeight, max(width - paletteWidth - layersWidth, 0),
        client.bottom - toolbarHeight - statusHeight, TRUE);
}
