/* DrawLite - Win32 Tutorial
 * Chapter 6 - The canvas
 *
 * File: canvas.c
 * The window you draw in. In this chapter it only draws a sample picture
 * using GDI, so we can learn how painting works.
 */

#include <windows.h>
#include "canvas.h"

#define CANVAS_CLASS    L"DrawLiteCanvas"
#define PAPER_WIDTH     640
#define PAPER_HEIGHT    480

static LRESULT CALLBACK Canvas_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Canvas_OnPaint(HWND hwnd);
static void Canvas_DrawPaper(HDC hdc, const RECT *paper);

static BOOL
Canvas_RegisterClass(HINSTANCE hInstance)
{
    static BOOL registered = FALSE;
    WNDCLASSEXW wcx;

    if (registered)
        return TRUE;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(wcx);
    wcx.style = CS_HREDRAW | CS_VREDRAW;    // Repaint the whole window when it is resized
    wcx.lpfnWndProc = Canvas_WndProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursorW(NULL, IDC_CROSS);
    wcx.hbrBackground = NULL;               // We paint the background ourselves
    wcx.lpszClassName = CANVAS_CLASS;

    registered = RegisterClassExW(&wcx) != 0;
    return registered;
}

HWND
Canvas_Create(HWND parent, HINSTANCE hInstance, int id)
{
    if (!Canvas_RegisterClass(hInstance))
        return NULL;

    return CreateWindowExW(0, CANVAS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, hInstance, NULL);
}

static LRESULT CALLBACK
Canvas_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        // Saying "we did it" stops Windows wiping the window before WM_PAINT.
        // That wipe is what makes unbuffered painting flicker.
        return 1;
    case WM_PAINT:
        Canvas_OnPaint(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Function: Canvas_OnPaint
// Draws the canvas. We draw everything into an off-screen bitmap first and
// copy the finished picture to the screen in one go (double buffering).
static void
Canvas_OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client, paper;
    int width, height;
    HDC memDC;
    HBITMAP memBitmap, oldBitmap;

    GetClientRect(hwnd, &client);
    width = client.right;
    height = client.bottom;

    // An off-screen DC with a bitmap the size of the window
    memDC = CreateCompatibleDC(hdc);
    memBitmap = CreateCompatibleBitmap(hdc, width, height);
    oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // The grey workspace
    FillRect(memDC, &client, GetSysColorBrush(COLOR_APPWORKSPACE));

    // A sheet of paper in the middle
    paper.left = (width - PAPER_WIDTH) / 2;
    paper.top = (height - PAPER_HEIGHT) / 2;
    paper.right = paper.left + PAPER_WIDTH;
    paper.bottom = paper.top + PAPER_HEIGHT;
    Canvas_DrawPaper(memDC, &paper);

    // Copy to the screen. Only the part that needs repainting.
    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
        ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
        memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);

    // Put back what we borrowed, then throw away what we made
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

// Function: Canvas_DrawPaper
// Draws a white sheet with a few GDI shapes on it.
static void
Canvas_DrawPaper(HDC hdc, const RECT *paper)
{
    int x = paper->left;
    int y = paper->top;
    RECT shadow = { paper->left + 4, paper->top + 4, paper->right + 4, paper->bottom + 4 };
    HPEN pen, dashedPen, oldPen;
    HBRUSH brush, oldBrush;
    HFONT font, oldFont;
    static const wchar_t text[] = L"Hello, GDI!";

    // Drop shadow, then the paper
    FillRect(hdc, &shadow, GetSysColorBrush(COLOR_3DSHADOW));
    FillRect(hdc, paper, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // A thick red line. To draw with GDI you create a pen, select it into the
    // DC (which hands you the old one), draw, then select the old pen back.
    pen = CreatePen(PS_SOLID, 5, RGB(200, 30, 30));
    oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x + 40, y + 40, NULL);
    LineTo(hdc, x + 300, y + 120);

    // A rectangle. It is outlined with the current pen and filled with the current brush.
    brush = CreateSolidBrush(RGB(60, 120, 220));
    oldBrush = (HBRUSH)SelectObject(hdc, brush);
    Rectangle(hdc, x + 60, y + 160, x + 260, y + 300);

    // An ellipse is the same, using the rectangle that would contain it.
    // This time we want a thin dashed outline and no fill.
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    dashedPen = CreatePen(PS_DASH, 1, RGB(0, 0, 0));
    SelectObject(hdc, dashedPen);   // The red pen is free now, but we still have to delete it
    Ellipse(hdc, x + 300, y + 160, x + 580, y + 300);

    // Some text
    font = CreateFontW(-48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(40, 40, 40));
    TextOutW(hdc, x + 40, y + 340, text, lstrlenW(text));

    // Give every borrowed object back, then delete the ones we created.
    // Forget this and you leak GDI objects until Windows runs out of them.
    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(font);
    DeleteObject(brush);
    DeleteObject(pen);
    DeleteObject(dashedPen);
}
