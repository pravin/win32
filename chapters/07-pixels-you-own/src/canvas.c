/* DrawLite - Win32 Tutorial
 * Chapter 7 - Pixels you own
 *
 * File: canvas.c
 * The window you draw in.
 */

#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include "canvas.h"

#define CANVAS_CLASS    L"DrawLiteCanvas"

// Struct: Canvas
// Everything the canvas window needs to remember.
typedef struct Canvas
{
    HWND hwnd;
    Document *doc;      // The picture we show. Not owned by us.
} Canvas;

static LRESULT CALLBACK Canvas_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Canvas_OnPaint(Canvas *cv);
static void Canvas_OnMouse(Canvas *cv, UINT msg, int x, int y, UINT keys);
static POINT Canvas_ImageOrigin(const Canvas *cv);

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
    wcx.hCursor = LoadCursorW(NULL, IDC_CROSS);
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

    hwnd = CreateWindowExW(0, CANVAS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, hInstance, cv);
    if (!hwnd)
        free(cv);
    return hwnd;
}

void
Canvas_SetDocument(HWND canvas, Document *doc)
{
    Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
    cv->doc = doc;
    InvalidateRect(canvas, NULL, FALSE);
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
    case WM_NCDESTROY:
        // The very last message a window receives. Time to free our state.
        free(cv);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Canvas_OnPaint(cv);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
        Canvas_OnMouse(cv, msg, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (UINT)wParam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Function: Canvas_ImageOrigin
// Where the top left pixel of the image is, in canvas window co-ordinates.
// For now the image is simply centred in the window.
static POINT
Canvas_ImageOrigin(const Canvas *cv)
{
    RECT client;
    POINT origin = { 0, 0 };

    GetClientRect(cv->hwnd, &client);
    if (cv->doc)
    {
        origin.x = (client.right - cv->doc->width) / 2;
        origin.y = (client.bottom - cv->doc->height) / 2;
    }
    return origin;
}

// Function: Canvas_OnMouse
// For now, painting a tiny black square wherever the mouse goes while the
// left button is down. It proves the pixels really are ours.
// Next chapter this turns into proper tools.
static void
Canvas_OnMouse(Canvas *cv, UINT msg, int x, int y, UINT keys)
{
    POINT origin;
    int ix, iy, dx, dy;

    if (!cv->doc)
        return;

    origin = Canvas_ImageOrigin(cv);
    ix = x - origin.x;
    iy = y - origin.y;

    // Tell the main window where we are, so it can show it in the status bar
    SendMessageW(GetParent(cv->hwnd), WMU_CANVAS_POS, (WPARAM)ix, (LPARAM)iy);

    switch (msg)
    {
    case WM_LBUTTONDOWN:
        SetCapture(cv->hwnd); // Keep getting mouse messages even outside the window
        break;
    case WM_LBUTTONUP:
        ReleaseCapture();
        return;
    }

    if (keys & MK_LBUTTON)
    {
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++)
                Surface_SetPixel(cv->doc->surface, ix + dx, iy + dy, 0xFF000000);
        cv->doc->modified = TRUE;
        InvalidateRect(cv->hwnd, NULL, FALSE);
    }
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
    memDC = CreateCompatibleDC(hdc);
    memBitmap = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    FillRect(memDC, &client, GetSysColorBrush(COLOR_APPWORKSPACE));

    if (cv->doc)
    {
        RECT shadow;
        origin = Canvas_ImageOrigin(cv);

        // Drop shadow
        shadow.left = origin.x + 4;
        shadow.top = origin.y + 4;
        shadow.right = shadow.left + cv->doc->width;
        shadow.bottom = shadow.top + cv->doc->height;
        FillRect(memDC, &shadow, GetSysColorBrush(COLOR_3DSHADOW));

        // The surface has a GDI bitmap behind it, so we can select it into a DC
        // and copy it like any other bitmap.
        imageDC = CreateCompatibleDC(hdc);
        oldImage = (HBITMAP)SelectObject(imageDC, cv->doc->surface->bitmap);
        BitBlt(memDC, origin.x, origin.y, cv->doc->width, cv->doc->height, imageDC, 0, 0, SRCCOPY);
        SelectObject(imageDC, oldImage);
        DeleteDC(imageDC);
    }

    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
        ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
        memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    EndPaint(cv->hwnd, &ps);
}
