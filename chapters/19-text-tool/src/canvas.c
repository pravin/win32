/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: canvas.c
 * The window you draw in.
 */

#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include "canvas.h"
#include "tools.h"
#include "composite.h"
#include "selection.h"
#include "texttool.h"

#define CANVAS_CLASS    L"DrawLiteCanvas"

// Empty space around the image when it is bigger than the window
#define CANVAS_MARGIN   16

// The zoom levels, in percent. Zoom in and out step through these.
static const int g_zoomLevels[] = { 25, 50, 100, 200, 300, 400, 600, 800, 1200, 1600, 3200 };
#define ZOOM_LEVELS     ((int)ARRAYSIZE(g_zoomLevels))
#define ZOOM_MIN        g_zoomLevels[0]
#define ZOOM_MAX        g_zoomLevels[ZOOM_LEVELS - 1]

// Zoom at which we start drawing a grid between the pixels
#define GRID_ZOOM       800

// The marching ants: a timer moves the dashes along the edge of the selection
#define ANTS_TIMER      1
#define ANTS_INTERVAL   120

// Struct: Canvas
// Everything the canvas window needs to remember.
typedef struct Canvas
{
    HWND hwnd;
    Document *doc;      // The picture we show. Not owned by us.
    ToolSettings *settings;   // Colours, brush size and so on. Owned by the main window.
    ToolState tool;     // The tool in action
    BOOL drawing;       // Is a mouse button down?
    int zoom;           // In percent. 100 is one image pixel per screen pixel.
    int scrollX;        // How far the view has moved right and down, in zoomed pixels
    int scrollY;
    BOOL panning;       // Dragging with the middle button?
    POINT panStart;     // Where the pan began, in window co-ordinates
    int panScrollX, panScrollY;
    BOOL inScrollUpdate; // Stops a loop when changing scroll bars changes our size
    int antsPhase;      // How far the marching ants have marched
    TextBox *text;      // The floating edit control, or NULL when nobody is typing
    BOOL textSecond;    // Was the text started with the right button? Then use the second colour
} Canvas;

static LRESULT CALLBACK Canvas_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Canvas_OnPaint(Canvas *cv);
static void Canvas_OnMouse(Canvas *cv, UINT msg, int x, int y, UINT keys);
static void Canvas_OnScroll(Canvas *cv, int bar, WPARAM wParam);
static void Canvas_OnWheel(Canvas *cv, int delta, UINT keys, BOOL horizontal, int x, int y);
static POINT Canvas_ImageOrigin(const Canvas *cv);
static void Canvas_ToImage(const Canvas *cv, int x, int y, int *ix, int *iy);
static void Canvas_InvalidateRect(const Canvas *cv, const RECT *area);
static void Canvas_RepaintDirty(Canvas *cv);
static void Canvas_FinishText(Canvas *cv, BOOL keep);
static void Canvas_StartText(Canvas *cv, int x, int y, BOOL second);
static BOOL Canvas_OnSetCursor(const Canvas *cv, int hitTest);
static void Canvas_UpdateScrollBars(Canvas *cv);
static void Canvas_ClampScroll(Canvas *cv);
static void Canvas_ScrollTo(Canvas *cv, int x, int y);
static void Canvas_DrawGrid(const Canvas *cv, HDC hdc, POINT origin, const RECT *area);
static void Canvas_DrawSelection(const Canvas *cv, HDC hdc, POINT origin, const RECT *area);

// Function: FloorDiv
// Integer division that rounds down. C rounds towards zero, which gives the
// wrong answer for negative numbers: -1 / 4 is 0 in C, but the pixel is -1.
static int
FloorDiv(int a, int b)
{
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        q--;
    return q;
}

static BOOL
Canvas_RegisterClass(HINSTANCE hInstance)
{
    static BOOL registered = FALSE;
    WNDCLASSEXW wcx;

    if (registered)
        return TRUE;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(wcx);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc = Canvas_WndProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = NULL;                     // We choose the cursor ourselves, see WM_SETCURSOR
    wcx.hbrBackground = NULL;
    wcx.lpszClassName = CANVAS_CLASS;

    registered = RegisterClassExW(&wcx) != 0;
    return registered;
}

HWND
Canvas_Create(HWND parent, HINSTANCE hInstance, int id)
{
    Canvas *cv;
    HWND hwnd;

    if (!Canvas_RegisterClass(hInstance))
        return NULL;

    cv = (Canvas *)calloc(1, sizeof(Canvas));
    if (!cv)
        return NULL;
    cv->zoom = 100;

    // WS_HSCROLL and WS_VSCROLL give the window scroll bars. Windows draws them for us.
    hwnd = CreateWindowExW(0, CANVAS_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, hInstance, cv);
    if (!hwnd)
        free(cv);
    return hwnd;
}

void
Canvas_SetDocument(HWND canvas, Document *doc)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    Canvas_FinishText(cv, TRUE);
    cv->doc = doc;
    cv->zoom = 100;
    cv->scrollX = 0;
    cv->scrollY = 0;
    Canvas_UpdateScrollBars(cv);
    SendMessageW(GetParent(canvas), WMU_ZOOM_CHANGED, (WPARAM)cv->zoom, 0);
    InvalidateRect(canvas, NULL, FALSE);
}

void
Canvas_SetSettings(HWND canvas, ToolSettings *settings)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    cv->settings = settings;
}

int
Canvas_GetZoom(HWND canvas)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    return cv->zoom;
}

void
Canvas_SetZoom(HWND canvas, int percent, int cx, int cy)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    POINT origin;
    RECT client;
    double fx, fy;

    if (!cv->doc)
        return;
    percent = max(ZOOM_MIN, min(ZOOM_MAX, percent));
    if (percent == cv->zoom)
        return;
    Canvas_FinishText(cv, TRUE);    // The box is sized for one zoom level only

    GetClientRect(canvas, &client);
    if (cx < 0 || cy < 0)
    {
        cx = client.right / 2;
        cy = client.bottom / 2;
    }

    // Which bit of the image is under the point we want to keep still?
    // We use doubles here so that the point does not creep about when zooming
    // in and out a lot.
    origin = Canvas_ImageOrigin(cv);
    fx = (cx - origin.x) * 100.0 / cv->zoom;
    fy = (cy - origin.y) * 100.0 / cv->zoom;

    cv->zoom = percent;

    // Now work out the scroll position that puts that bit back under (cx, cy).
    // If the image is smaller than the window, the clamp will undo this and
    // the image will be centred, which is what we want.
    cv->scrollX = CANVAS_MARGIN - (cx - (int)(fx * percent / 100.0));
    cv->scrollY = CANVAS_MARGIN - (cy - (int)(fy * percent / 100.0));
    Canvas_ClampScroll(cv);
    Canvas_UpdateScrollBars(cv);
    InvalidateRect(canvas, NULL, FALSE);
    SendMessageW(GetParent(canvas), WMU_ZOOM_CHANGED, (WPARAM)cv->zoom, 0);
}

void
Canvas_ZoomStep(HWND canvas, int direction, int cx, int cy)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    int i;

    if (direction > 0)
    {
        for (i = 0; i < ZOOM_LEVELS; i++)
            if (g_zoomLevels[i] > cv->zoom)
            {
                Canvas_SetZoom(canvas, g_zoomLevels[i], cx, cy);
                return;
            }
    }
    else
    {
        for (i = ZOOM_LEVELS - 1; i >= 0; i--)
            if (g_zoomLevels[i] < cv->zoom)
            {
                Canvas_SetZoom(canvas, g_zoomLevels[i], cx, cy);
                return;
            }
    }
}

void
Canvas_ZoomToFit(HWND canvas)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    RECT client;
    int best, i;

    if (!cv->doc)
        return;
    GetClientRect(canvas, &client);

    // The biggest level at which the image still fits. Always at least the smallest.
    best = ZOOM_MIN;
    for (i = 0; i < ZOOM_LEVELS; i++)
    {
        int w = cv->doc->width * g_zoomLevels[i] / 100 + 2 * CANVAS_MARGIN;
        int h = cv->doc->height * g_zoomLevels[i] / 100 + 2 * CANVAS_MARGIN;
        if (w <= client.right && h <= client.bottom)
            best = g_zoomLevels[i];
    }
    Canvas_SetZoom(canvas, best, -1, -1);
}

static LRESULT CALLBACK
Canvas_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        cv = (Canvas *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        cv->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cv);
        break;
    case WM_CREATE:
        SetTimer(hwnd, ANTS_TIMER, ANTS_INTERVAL, NULL);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, ANTS_TIMER);
        return 0;
    case WM_TIMER:
        if (wParam == ANTS_TIMER)
        {
            // Move the ants along and repaint just the part of the picture with the outline
            if (cv->doc && Sel_IsActive(&cv->doc->sel))
            {
                RECT r = cv->doc->sel.bounds;
                InflateRect(&r, 1, 1);
                cv->antsPhase = (cv->antsPhase + 2) & 7;
                Canvas_InvalidateRect(cv, &r);
            }
            return 0;
        }
        break;
    case WMU_TEXT_DONE:
        // Ignore news from a text box that has already gone
        if (cv->text && cv->text->serial == (UINT)wParam)
            Canvas_FinishText(cv, (BOOL)lParam);
        return 0;
    case WM_CTLCOLOREDIT:
        // The edit control asks us what colours to use. Hand it the ones for the text colour.
        if (cv->text && (HWND)lParam == cv->text->hwnd)
        {
            SetTextColor((HDC)wParam, cv->text->textColor);
            SetBkColor((HDC)wParam, cv->text->backColor);
            return (LRESULT)cv->text->background;
        }
        break;
    case WM_NCDESTROY:
        // The very last message a window receives. Time to free our state.
        Text_Destroy(cv->text);
        cv->text = NULL;
        Tool_Release(&cv->tool);
        free(cv);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    case WM_SIZE:
        Canvas_ClampScroll(cv);
        Canvas_UpdateScrollBars(cv);
        return 0;
    case WM_HSCROLL:
        Canvas_OnScroll(cv, SB_HORZ, wParam);
        return 0;
    case WM_VSCROLL:
        Canvas_OnScroll(cv, SB_VERT, wParam);
        return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    {
        // Wheel messages carry the position in SCREEN co-ordinates
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        Canvas_OnWheel(cv, GET_WHEEL_DELTA_WPARAM(wParam), GET_KEYSTATE_WPARAM(wParam),
            msg == WM_MOUSEHWHEEL, pt.x, pt.y);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        if (Canvas_OnSetCursor(cv, LOWORD(lParam)))
            return TRUE;
        break;
    case WM_PAINT:
        Canvas_OnPaint(cv);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        Canvas_OnMouse(cv, msg, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)wParam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Function: Canvas_ImageOrigin
// Where the top left pixel of the image is, in canvas window co-ordinates.
// If the image (zoomed) is smaller than the window it sits in the middle.
// Otherwise the scroll position decides.
static POINT
Canvas_ImageOrigin(const Canvas *cv)
{
    RECT client;
    POINT origin = { 0, 0 };

    GetClientRect(cv->hwnd, &client);
    if (cv->doc)
    {
        int zw = cv->doc->width * cv->zoom / 100;
        int zh = cv->doc->height * cv->zoom / 100;

        if (zw + 2 * CANVAS_MARGIN <= client.right)
            origin.x = (client.right - zw) / 2;
        else
            origin.x = CANVAS_MARGIN - cv->scrollX;

        if (zh + 2 * CANVAS_MARGIN <= client.bottom)
            origin.y = (client.bottom - zh) / 2;
        else
            origin.y = CANVAS_MARGIN - cv->scrollY;
    }
    return origin;
}

// Function: Canvas_ToImage
// Turns a position in the window into a pixel in the image.
static void
Canvas_ToImage(const Canvas *cv, int x, int y, int *ix, int *iy)
{
    POINT origin = Canvas_ImageOrigin(cv);
    *ix = FloorDiv((x - origin.x) * 100, cv->zoom);
    *iy = FloorDiv((y - origin.y) * 100, cv->zoom);
}

// Function: Canvas_ClampScroll
// Keeps the scroll position inside the image.
static void
Canvas_ClampScroll(Canvas *cv)
{
    RECT client;
    int rangeX, rangeY;

    if (!cv->doc)
        return;
    GetClientRect(cv->hwnd, &client);
    rangeX = cv->doc->width * cv->zoom / 100 + 2 * CANVAS_MARGIN;
    rangeY = cv->doc->height * cv->zoom / 100 + 2 * CANVAS_MARGIN;
    cv->scrollX = max(0, min(cv->scrollX, rangeX - client.right));
    cv->scrollY = max(0, min(cv->scrollY, rangeY - client.bottom));
}

// Function: Canvas_UpdateScrollBars
// Tells Windows how big the scrollable area is, how much of it we can see,
// and where we are.
static void
Canvas_UpdateScrollBars(Canvas *cv)
{
    RECT client;
    SCROLLINFO si;

    if (!cv->doc || cv->inScrollUpdate)
        return;

    // Showing or hiding a scroll bar changes our client size, and that sends
    // us another WM_SIZE, which brings us back here. This flag breaks the loop.
    cv->inScrollUpdate = TRUE;

    GetClientRect(cv->hwnd, &client);
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    si.nMin = 0;
    si.nMax = cv->doc->width * cv->zoom / 100 + 2 * CANVAS_MARGIN - 1;
    si.nPage = client.right;
    si.nPos = cv->scrollX;
    SetScrollInfo(cv->hwnd, SB_HORZ, &si, TRUE);

    si.nMax = cv->doc->height * cv->zoom / 100 + 2 * CANVAS_MARGIN - 1;
    si.nPage = client.bottom;
    si.nPos = cv->scrollY;
    SetScrollInfo(cv->hwnd, SB_VERT, &si, TRUE);

    cv->inScrollUpdate = FALSE;
}

// Function: Canvas_ScrollTo
// Moves the view, repaints, and moves the scroll bars to match.
static void
Canvas_ScrollTo(Canvas *cv, int x, int y)
{
    Canvas_FinishText(cv, TRUE);
    int oldX = cv->scrollX, oldY = cv->scrollY;

    cv->scrollX = x;
    cv->scrollY = y;
    Canvas_ClampScroll(cv);
    if (cv->scrollX == oldX && cv->scrollY == oldY)
        return;

    Canvas_UpdateScrollBars(cv);
    // Slide what is already on screen and only paint the strip that is new.
    // ScrollWindowEx tells Windows to invalidate that strip for us.
    ScrollWindowEx(cv->hwnd, oldX - cv->scrollX, oldY - cv->scrollY, NULL, NULL, NULL, NULL, SW_INVALIDATE);
}

// Function: Canvas_OnScroll
// The user dragged the thumb or clicked the arrows of a scroll bar.
static void
Canvas_OnScroll(Canvas *cv, int bar, WPARAM wParam)
{
    SCROLLINFO si;
    int pos;

    // Get the full 32 bit track position. LOWORD(wParam) is only 16 bits.
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(cv->hwnd, bar, &si);
    pos = si.nPos;

    switch (LOWORD(wParam))
    {
    case SB_LINEUP:         pos -= 40; break;       // Same name for left and up
    case SB_LINEDOWN:       pos += 40; break;
    case SB_PAGEUP:         pos -= (int)si.nPage; break;
    case SB_PAGEDOWN:       pos += (int)si.nPage; break;
    case SB_THUMBTRACK:     pos = si.nTrackPos; break;
    case SB_TOP:            pos = si.nMin; break;
    case SB_BOTTOM:         pos = si.nMax; break;
    default:                return;
    }

    if (bar == SB_HORZ)
        Canvas_ScrollTo(cv, pos, cv->scrollY);
    else
        Canvas_ScrollTo(cv, cv->scrollX, pos);
}

// Function: Canvas_OnWheel
// Wheel scrolls. Shift+wheel scrolls sideways. Ctrl+wheel zooms.
static void
Canvas_OnWheel(Canvas *cv, int delta, UINT keys, BOOL horizontal, int x, int y)
{
    // One notch of the wheel is WHEEL_DELTA (120). Some mice send smaller steps.
    int step = delta * 80 / WHEEL_DELTA;

    if (!cv->doc)
        return;
    if (keys & MK_CONTROL)
    {
        Canvas_ZoomStep(cv->hwnd, delta > 0 ? 1 : -1, x, y);
        return;
    }
    if (horizontal || (keys & MK_SHIFT))
        Canvas_ScrollTo(cv, cv->scrollX + (horizontal ? step : -step), cv->scrollY);
    else
        Canvas_ScrollTo(cv, cv->scrollX, cv->scrollY - step);
}

// Function: Canvas_InvalidateRect
// Asks Windows to repaint the part of the window that shows this part of the image.
static void
Canvas_InvalidateRect(const Canvas *cv, const RECT *area)
{
    POINT origin = Canvas_ImageOrigin(cv);
    RECT r;

    // Round outwards, so that a partly covered screen pixel is included
    r.left = origin.x + area->left * cv->zoom / 100;
    r.top = origin.y + area->top * cv->zoom / 100;
    r.right = origin.x + (area->right * cv->zoom + 99) / 100 + 1;
    r.bottom = origin.y + (area->bottom * cv->zoom + 99) / 100 + 1;
    InvalidateRect(cv->hwnd, &r, FALSE);
}

// Function: Canvas_RepaintDirty
// Repaints whatever the current tool has changed since we last looked.
static void
Canvas_RepaintDirty(Canvas *cv)
{
    RECT area;
    if (Tool_TakeDirty(&cv->tool, &area))
    {
        // The tool drew on one layer. Update what we show, then repaint that part.
        Doc_UpdateView(cv->doc, &area);
        Canvas_InvalidateRect(cv, &area);
    }
    // A selection tool only changed the outline. No need to recombine the layers.
    if (Tool_TakeOverlay(&cv->tool, &area))
    {
        InflateRect(&area, 1, 1);
        Canvas_InvalidateRect(cv, &area);
    }
}

// Function: Canvas_StartText
// Puts a text box at window position (x, y). The top left corner of the text
// lands on the image pixel under the mouse.
static void
Canvas_StartText(Canvas *cv, int x, int y, BOOL second)
{
    POINT origin = Canvas_ImageOrigin(cv);
    int ix, iy;
    LOGFONTW lf = cv->settings->font;

    Canvas_ToImage(cv, x, y, &ix, &iy);
    if (ix < 0 || iy < 0 || ix >= cv->doc->width || iy >= cv->doc->height)
        return;     // Text starting outside the picture would be mostly lost
    // Snap the box to the pixel grid, so what you see is where the text goes
    cv->text = Text_Create(cv->hwnd, &lf, second ? cv->settings->secondary : cv->settings->primary,
        cv->zoom, origin.x + ix * cv->zoom / 100, origin.y + iy * cv->zoom / 100, ix, iy);
    cv->textSecond = second;
}

// Function: Canvas_FinishText
// Ends the text box. If keep is TRUE what was typed is painted onto the active
// layer as one undo step, otherwise it is thrown away.
static void
Canvas_FinishText(Canvas *cv, BOOL keep)
{
    TextBox *tb = cv->text;
    wchar_t *str;
    uint8_t *mask;
    int w, h;
    BOOL changed = FALSE;

    if (!tb)
        return;
    // Take the box out of the canvas first. Destroying it moves the keyboard focus,
    // which would tell us to finish it again.
    cv->text = NULL;
    str = keep ? Text_GetString(tb) : NULL;
    if (str)
    {
        mask = Text_Rasterize(&tb->logFont, str, &w, &h);
        if (mask)
        {
            ToolSettings tmp = *cv->settings;
            tmp.primary = tb->color;
            changed = Tool_PaintText(&cv->tool, cv->doc, &tmp, FALSE, tb->imageX, tb->imageY, w, h, mask);
            free(mask);
        }
        free(str);
    }
    Text_Destroy(tb);
    SetFocus(cv->hwnd);
    if (changed)
    {
        Canvas_RepaintDirty(cv);
        SendMessageW(GetParent(cv->hwnd), WMU_DOC_CHANGED, 0, 0);
    }
}

void
Canvas_CommitText(HWND canvas)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    Canvas_FinishText(cv, TRUE);
}

BOOL
Canvas_IsBusy(HWND canvas)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    return cv->drawing || cv->panning;
}

void
Canvas_InvalidateImageRect(HWND canvas, const RECT *area)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    Canvas_InvalidateRect(cv, area);
}

// Function: Canvas_OnSetCursor
// Windows asks which mouse pointer to show. Over the image we want the
// cross hair, anywhere else the normal arrow.
//
// Returns:
//   TRUE if we set the cursor, FALSE to let Windows do it.
static BOOL
Canvas_OnSetCursor(const Canvas *cv, int hitTest)
{
    POINT pt;
    int ix, iy;
    BOOL overImage;
    LPCWSTR cursor;

    if (hitTest != HTCLIENT || !cv->doc)
        return FALSE;

    GetCursorPos(&pt);                  // Screen co-ordinates
    ScreenToClient(cv->hwnd, &pt);      // Now window co-ordinates
    Canvas_ToImage(cv, pt.x, pt.y, &ix, &iy);
    overImage = ix >= 0 && iy >= 0 && ix < cv->doc->width && iy < cv->doc->height;

    if (cv->panning)
        cursor = IDC_SIZEALL;
    else
        // While drawing, keep the cross hair even if we stray off the edge
        cursor = (overImage || cv->drawing) ? (cv->settings && cv->settings->tool == TOOL_TEXT ? IDC_IBEAM : IDC_CROSS) : IDC_ARROW;
    SetCursor(LoadCursorW(NULL, cursor));
    return TRUE;
}

// Function: Canvas_OnMouse
// Turns mouse messages into tool calls.
static void
Canvas_OnMouse(Canvas *cv, UINT msg, int x, int y, UINT keys)
{
    int ix, iy;

    if (!cv->doc || !cv->settings)
        return;

    // The middle button pans. It never reaches the tools.
    if (msg == WM_MBUTTONDOWN && !cv->drawing)
    {
        cv->panning = TRUE;
        cv->panStart.x = x;
        cv->panStart.y = y;
        cv->panScrollX = cv->scrollX;
        cv->panScrollY = cv->scrollY;
        SetCapture(cv->hwnd);
        return;
    }
    if (cv->panning)
    {
        if (msg == WM_MOUSEMOVE)
            Canvas_ScrollTo(cv, cv->panScrollX - (x - cv->panStart.x), cv->panScrollY - (y - cv->panStart.y));
        else if (msg == WM_MBUTTONUP)
        {
            cv->panning = FALSE;
            ReleaseCapture();
        }
        return;
    }

    Canvas_ToImage(cv, x, y, &ix, &iy);

    // The text tool does not draw while the mouse is down. A click puts a text box there.
    if (cv->settings->tool == TOOL_TEXT && (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) && !cv->drawing)
    {
        Canvas_FinishText(cv, TRUE);    // Whatever was being typed is done
        Canvas_StartText(cv, x, y, msg == WM_RBUTTONDOWN);
        return;
    }

    // Tell the main window where we are, so it can show it in the status bar
    SendMessageW(GetParent(cv->hwnd), WMU_CANVAS_POS, (WPARAM)ix, (LPARAM)iy);

    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (cv->drawing)
            return; // Already drawing with the other button
        cv->drawing = TRUE;
        SetCapture(cv->hwnd); // Keep getting mouse messages even outside the window
        Tool_MouseDown(&cv->tool, cv->doc, cv->settings, ix, iy, msg == WM_RBUTTONDOWN, keys);
        break;
    case WM_MOUSEMOVE:
        if (cv->drawing)
            Tool_MouseMove(&cv->tool, cv->doc, cv->settings, ix, iy, keys);
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        if (cv->drawing)
        {
            Tool_MouseUp(&cv->tool, cv->doc, cv->settings, ix, iy, keys);
            cv->drawing = FALSE;
            ReleaseCapture();
            SendMessageW(GetParent(cv->hwnd), WMU_DOC_CHANGED, 0, 0);
        }
        break;
    }
    Canvas_RepaintDirty(cv);
    if (cv->tool.colorsChanged)
    {
        cv->tool.colorsChanged = FALSE;
        SendMessageW(GetParent(cv->hwnd), WMU_COLORS_CHANGED, 0, 0);
    }
}

// Function: Canvas_DrawGrid
// When zoomed right in, draws thin lines between the pixels.
static void
Canvas_DrawGrid(const Canvas *cv, HDC hdc, POINT origin, const RECT *area)
{
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    int x0 = FloorDiv((area->left - origin.x) * 100, cv->zoom);
    int x1 = FloorDiv((area->right - origin.x) * 100, cv->zoom) + 1;
    int y0 = FloorDiv((area->top - origin.y) * 100, cv->zoom);
    int y1 = FloorDiv((area->bottom - origin.y) * 100, cv->zoom) + 1;
    int i;

    x0 = max(x0, 0);
    y0 = max(y0, 0);
    x1 = min(x1, cv->doc->width);
    y1 = min(y1, cv->doc->height);

    for (i = x0; i <= x1; i++)
    {
        int sx = origin.x + i * cv->zoom / 100;
        MoveToEx(hdc, sx, origin.y + y0 * cv->zoom / 100, NULL);
        LineTo(hdc, sx, origin.y + y1 * cv->zoom / 100);
    }
    for (i = y0; i <= y1; i++)
    {
        int sy = origin.y + i * cv->zoom / 100;
        MoveToEx(hdc, origin.x + x0 * cv->zoom / 100, sy, NULL);
        LineTo(hdc, origin.x + x1 * cv->zoom / 100, sy);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

// Function: Ants_Segment
// Draws one straight, horizontal or vertical stretch of the outline: a solid
// white line with black dashes on top. The dashes are placed by their position
// on screen (plus the phase), so a long edge made of many little pieces still
// has even dashes, and changing the phase makes them march.
static void
Ants_Segment(HDC hdc, HPEN white, HPEN black, int phase, int x0, int y0, int x1, int y1)
{
    BOOL horizontal = (y0 == y1);
    int a = horizontal ? x0 : y0;
    int b = horizontal ? x1 : y1;
    int start;

    SelectObject(hdc, white);
    MoveToEx(hdc, x0, y0, NULL);
    LineTo(hdc, x1, y1);

    SelectObject(hdc, black);
    // The dash pattern repeats every 8 pixels: 4 black, 4 white
    start = a - ((((a + phase) % 8) + 8) % 8);
    for (; start < b; start += 8)
    {
        int d0 = max(start, a), d1 = min(start + 4, b);
        if (d0 >= d1)
            continue;
        if (horizontal)
        {
            MoveToEx(hdc, d0, y0, NULL);
            LineTo(hdc, d1, y0);
        }
        else
        {
            MoveToEx(hdc, x0, d0, NULL);
            LineTo(hdc, x0, d1);
        }
    }
}

// Function: Canvas_DrawSelection
// Draws the outline of the selection: every place where a selected pixel
// touches an unselected one.
static void
Canvas_DrawSelection(const Canvas *cv, HDC hdc, POINT origin, const RECT *area)
{
    const Selection *sel = &cv->doc->sel;
    int w = cv->doc->width, h = cv->doc->height;
    HPEN white, black, oldPen;
    RECT r;
    int x, y, runStart;

    if (!sel->mask)
        return;

    // Only the part of the picture that is both selected-ish and on screen
    r.left = FloorDiv((area->left - origin.x) * 100, cv->zoom) - 1;
    r.top = FloorDiv((area->top - origin.y) * 100, cv->zoom) - 1;
    r.right = FloorDiv((area->right - origin.x) * 100, cv->zoom) + 2;
    r.bottom = FloorDiv((area->bottom - origin.y) * 100, cv->zoom) + 2;
    {
        RECT b = sel->bounds;
        InflateRect(&b, 1, 1);
        if (!IntersectRect(&r, &r, &b))
            return;
    }
    r.left = max(r.left, 0);
    r.top = max(r.top, 0);
    r.right = min(r.right, w);
    r.bottom = min(r.bottom, h);

    white = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    black = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    oldPen = (HPEN)SelectObject(hdc, white);

#define IS_SEL(px, py) ((px) >= 0 && (py) >= 0 && (px) < w && (py) < h && sel->mask[(size_t)(py) * w + (px)] >= 128)
#define SX(px) (origin.x + (px) * cv->zoom / 100)
#define SY(py) (origin.y + (py) * cv->zoom / 100)

    // Horizontal edges: between row y - 1 and row y
    for (y = r.top; y <= r.bottom; y++)
    {
        runStart = -1;
        for (x = r.left; x <= r.right; x++)
        {
            BOOL edge = x < r.right && IS_SEL(x, y - 1) != IS_SEL(x, y);
            if (edge && runStart < 0)
                runStart = x;
            else if (!edge && runStart >= 0)
            {
                Ants_Segment(hdc, white, black, cv->antsPhase, SX(runStart), SY(y), SX(x), SY(y));
                runStart = -1;
            }
        }
    }
    // Vertical edges: between column x - 1 and column x
    for (x = r.left; x <= r.right; x++)
    {
        runStart = -1;
        for (y = r.top; y <= r.bottom; y++)
        {
            BOOL edge = y < r.bottom && IS_SEL(x - 1, y) != IS_SEL(x, y);
            if (edge && runStart < 0)
                runStart = y;
            else if (!edge && runStart >= 0)
            {
                Ants_Segment(hdc, white, black, cv->antsPhase, SX(x), SY(runStart), SX(x), SY(y));
                runStart = -1;
            }
        }
    }
#undef IS_SEL
#undef SX
#undef SY

    SelectObject(hdc, oldPen);
    DeleteObject(white);
    DeleteObject(black);
}

// Function: Canvas_OnPaint
// Draws the grey workspace and the image, using a back buffer to avoid flicker.
static void
Canvas_OnPaint(Canvas *cv)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(cv->hwnd, &ps);
    RECT client;
    HDC memDC, imageDC;
    HBITMAP memBitmap, oldBitmap, oldImage;
    POINT origin;

    GetClientRect(cv->hwnd, &client);
    // The back buffer only needs to be as big as the area being repainted.
    // A tiny brush stroke should not allocate a window-sized bitmap.
    int paintW = ps.rcPaint.right - ps.rcPaint.left;
    int paintH = ps.rcPaint.bottom - ps.rcPaint.top;

    if (paintW <= 0 || paintH <= 0)
    {
        EndPaint(cv->hwnd, &ps);
        return;
    }
    memDC = CreateCompatibleDC(hdc);
    memBitmap = CreateCompatibleBitmap(hdc, paintW, paintH);
    oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
    // Shift the origin so we can keep drawing in window co-ordinates
    SetViewportOrgEx(memDC, -ps.rcPaint.left, -ps.rcPaint.top, NULL);

    FillRect(memDC, &client, GetSysColorBrush(COLOR_APPWORKSPACE));

    if (cv->doc)
    {
        RECT shadow, imageRect, visible;
        int zw = cv->doc->width * cv->zoom / 100;
        int zh = cv->doc->height * cv->zoom / 100;
        origin = Canvas_ImageOrigin(cv);

        // Drop shadow
        shadow.left = origin.x + 4;
        shadow.top = origin.y + 4;
        shadow.right = shadow.left + zw;
        shadow.bottom = shadow.top + zh;
        FillRect(memDC, &shadow, GetSysColorBrush(COLOR_3DSHADOW));

        // Which part of the image is inside the area we are repainting?
        SetRect(&imageRect, origin.x, origin.y, origin.x + zw, origin.y + zh);
        if (IntersectRect(&visible, &imageRect, &ps.rcPaint))
        {
            // Work out which image pixels that covers, rounding outwards to
            // whole pixels. Stretching whole pixels keeps the picture steady
            // however we are scrolled.
            int sx0 = FloorDiv((visible.left - origin.x) * 100, cv->zoom);
            int sy0 = FloorDiv((visible.top - origin.y) * 100, cv->zoom);
            int sx1 = FloorDiv((visible.right - origin.x) * 100 + cv->zoom - 1, cv->zoom);
            int sy1 = FloorDiv((visible.bottom - origin.y) * 100 + cv->zoom - 1, cv->zoom);

            sx1 = min(sx1, cv->doc->width);
            sy1 = min(sy1, cv->doc->height);

            // The surface has a GDI bitmap behind it, so we can select it into a DC
            // and copy it like any other bitmap. StretchBlt does the zooming.
            imageDC = CreateCompatibleDC(hdc);
            oldImage = (HBITMAP)SelectObject(imageDC, cv->doc->view->bitmap);
            // COLORONCOLOR just throws pixels away or repeats them. No blurring.
            SetStretchBltMode(memDC, COLORONCOLOR);
            StretchBlt(memDC,
                origin.x + sx0 * cv->zoom / 100, origin.y + sy0 * cv->zoom / 100,
                (sx1 - sx0) * cv->zoom / 100, (sy1 - sy0) * cv->zoom / 100,
                imageDC, sx0, sy0, sx1 - sx0, sy1 - sy0, SRCCOPY);
            SelectObject(imageDC, oldImage);
            DeleteDC(imageDC);

            if (cv->zoom >= GRID_ZOOM)
                Canvas_DrawGrid(cv, memDC, origin, &visible);
            Canvas_DrawSelection(cv, memDC, origin, &visible);
        }
    }

    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, paintW, paintH,
        memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    EndPaint(cv->hwnd, &ps);
}
