/* DrawLite - Win32 Tutorial
 * Chapter 14 - Open and save
 *
 * File: palette.c
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdlib.h>
#include <strsafe.h>
#include "palette.h"
#include "pixel.h"
#include "resource.h"
#include "uiutil.h"

#define PALETTE_CLASS   L"DrawLitePalette"
#define QUICK_COLS      8
#define QUICK_ROWS      2

// The sixteen quick colours, as COLORREFs (0x00BBGGRR)
static const COLORREF g_quickColors[QUICK_COLS * QUICK_ROWS] =
{
    RGB(0, 0, 0),       RGB(128, 128, 128), RGB(128, 0, 0),     RGB(128, 128, 0),
    RGB(0, 128, 0),     RGB(0, 128, 128),   RGB(0, 0, 128),     RGB(128, 0, 128),
    RGB(255, 255, 255), RGB(192, 192, 192), RGB(255, 0, 0),     RGB(255, 255, 0),
    RGB(0, 255, 0),     RGB(0, 255, 255),   RGB(0, 0, 255),     RGB(255, 0, 255)
};

// Struct: Palette
typedef struct Palette
{
    HWND hwnd;
    ToolSettings *settings;
    HWND hAlpha;                // The alpha trackbar
    HWND hAlphaLabel;           // "Alpha: 255"
    HWND hSwap;                 // The Swap button
    COLORREF customColors[16];  // Colours the user has mixed in the colour dialog
} Palette;

// Struct: PaletteLayout
// Where everything sits, in window co-ordinates.
typedef struct PaletteLayout
{
    RECT primary;
    RECT secondary;
    RECT swap;
    RECT quick;         // The whole grid of quick colours
    int cell;           // Size of one quick colour cell
    RECT alphaLabel;
    RECT alphaBar;
} PaletteLayout;

static LRESULT CALLBACK Palette_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Palette_Layout(Palette *p);
static void Palette_GetLayout(const Palette *p, PaletteLayout *lay);
static void Palette_OnPaint(Palette *p);
static void Palette_OnClick(Palette *p, int x, int y, BOOL rightButton);
static void Palette_PickColor(Palette *p, BOOL primary);
static void Palette_UpdateAlphaLabel(Palette *p);
static void Palette_DrawSwatch(HDC hdc, const RECT *r, uint32_t color);

static BOOL
Palette_RegisterClass(HINSTANCE hInstance)
{
    static BOOL registered = FALSE;
    WNDCLASSEXW wcx;

    if (registered)
        return TRUE;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(wcx);
    wcx.lpfnWndProc = Palette_WndProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcx.lpszClassName = PALETTE_CLASS;

    registered = RegisterClassExW(&wcx) != 0;
    return registered;
}

HWND
Palette_Create(HWND parent, HINSTANCE hInstance, int id, ToolSettings *settings)
{
    Palette *p;
    HWND hwnd;

    if (!Palette_RegisterClass(hInstance))
        return NULL;

    p = (Palette *)calloc(1, sizeof(Palette));
    if (!p)
        return NULL;
    p->settings = settings;

    hwnd = CreateWindowExW(0, PALETTE_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, hInstance, p);
    if (!hwnd)
        free(p);
    return hwnd;
}

// Function: Palette_OnCreate
// Makes the buttons, the slider and the label.
static BOOL
Palette_OnCreate(Palette *p)
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(p->hwnd, GWLP_HINSTANCE);
    int i;

    // One push-like radio button per tool. Radio buttons give us
    // "only one pressed at a time" for free.
    for (i = 0; i < TOOL_COUNT; i++)
    {
        DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE;
        if (i == 0)
            style |= WS_GROUP | WS_TABSTOP;
        CreateWindowExW(0, L"BUTTON", Tool_Name((ToolId)i), style, 0, 0, 0, 0,
            p->hwnd, (HMENU)(INT_PTR)(IDM_TOOL_FIRST + i), hInstance, NULL);
    }

    p->hSwap = CreateWindowExW(0, L"BUTTON", L"Swap", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_SWAP, hInstance, NULL);
    p->hAlphaLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_ALPHA_LABEL, hInstance, NULL);
    p->hAlpha = CreateWindowExW(0, TRACKBAR_CLASSW, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_ALPHA, hInstance, NULL);
    if (!p->hSwap || !p->hAlphaLabel || !p->hAlpha)
        return FALSE;

    SendMessageW(p->hAlpha, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessageW(p->hAlpha, TBM_SETPOS, TRUE, PIX_A(p->settings->primary));
    Palette_UpdateAlphaLabel(p);
    return TRUE;
}

void
Palette_SetTool(HWND palette, ToolId tool)
{
    // Pressing one of the group's buttons un-presses the others for us
    CheckRadioButton(palette, IDM_TOOL_FIRST, IDM_TOOL_FIRST + TOOL_COUNT - 1, IDM_TOOL_FIRST + tool);
}

void
Palette_Refresh(HWND palette)
{
    Palette *p = (Palette *)GetWindowLongPtrW(palette, GWLP_USERDATA);
    SendMessageW(p->hAlpha, TBM_SETPOS, TRUE, PIX_A(p->settings->primary));
    Palette_UpdateAlphaLabel(p);
    InvalidateRect(palette, NULL, FALSE);
}

void
Palette_SwapColors(HWND palette)
{
    Palette *p = (Palette *)GetWindowLongPtrW(palette, GWLP_USERDATA);
    uint32_t tmp = p->settings->primary;
    p->settings->primary = p->settings->secondary;
    p->settings->secondary = tmp;
    Palette_Refresh(palette);
}

// Function: Palette_GetLayout
// Works out where everything goes. Used for laying out the child windows,
// for painting, and for working out what was clicked, so they always agree.
static void
Palette_GetLayout(const Palette *p, PaletteLayout *lay)
{
    UINT dpi = GetDpiForWindow(p->hwnd);
    int margin = Ui_Scale(dpi, 6);
    int toolRows = (TOOL_COUNT + 1) / 2;
    int toolsBottom = margin + toolRows * (Ui_Scale(dpi, 28) + margin / 2);
    int swatch = Ui_Scale(dpi, 36);
    int overlap = Ui_Scale(dpi, 22);
    int top = toolsBottom + Ui_Scale(dpi, 12);
    RECT client;

    GetClientRect(p->hwnd, &client);

    // The secondary swatch sits behind and to the right of the primary one
    SetRect(&lay->secondary, margin + overlap, top + overlap, margin + overlap + swatch, top + overlap + swatch);
    SetRect(&lay->primary, margin, top, margin + swatch, top + swatch);
    SetRect(&lay->swap, margin + overlap + swatch + margin, top, client.right - margin, top + Ui_Scale(dpi, 24));

    lay->cell = (client.right - 2 * margin) / QUICK_COLS;
    SetRect(&lay->quick, margin, lay->secondary.bottom + Ui_Scale(dpi, 12),
        margin + lay->cell * QUICK_COLS, lay->secondary.bottom + Ui_Scale(dpi, 12) + lay->cell * QUICK_ROWS);

    SetRect(&lay->alphaLabel, margin, lay->quick.bottom + Ui_Scale(dpi, 10),
        client.right - margin, lay->quick.bottom + Ui_Scale(dpi, 28));
    SetRect(&lay->alphaBar, margin, lay->alphaLabel.bottom,
        client.right - margin, lay->alphaLabel.bottom + Ui_Scale(dpi, 28));
}

// Function: Palette_Layout
// Moves the child windows to their places.
static void
Palette_Layout(Palette *p)
{
    RECT rc;
    PaletteLayout lay;
    UINT dpi = GetDpiForWindow(p->hwnd);
    int margin = Ui_Scale(dpi, 6);
    int height = Ui_Scale(dpi, 28);
    int width, i;

    GetClientRect(p->hwnd, &rc);
    width = (rc.right - 3 * margin) / 2; // two buttons and three gaps across
    for (i = 0; i < TOOL_COUNT; i++)
    {
        HWND button = GetDlgItem(p->hwnd, IDM_TOOL_FIRST + i);
        int col = i % 2;
        int row = i / 2;
        MoveWindow(button, margin + col * (width + margin), margin + row * (height + margin / 2),
            width, height, TRUE);
    }

    Palette_GetLayout(p, &lay);
    MoveWindow(p->hSwap, lay.swap.left, lay.swap.top, lay.swap.right - lay.swap.left, lay.swap.bottom - lay.swap.top, TRUE);
    MoveWindow(p->hAlphaLabel, lay.alphaLabel.left, lay.alphaLabel.top,
        lay.alphaLabel.right - lay.alphaLabel.left, lay.alphaLabel.bottom - lay.alphaLabel.top, TRUE);
    MoveWindow(p->hAlpha, lay.alphaBar.left, lay.alphaBar.top,
        lay.alphaBar.right - lay.alphaBar.left, lay.alphaBar.bottom - lay.alphaBar.top, TRUE);
}

// Function: Palette_DrawSwatch
// Fills a rectangle with a colour, including its see-through-ness.
// Behind a see-through colour we draw a little chequerboard, then
// work out for each square what the colour looks like on top of it.
static void
Palette_DrawSwatch(HDC hdc, const RECT *r, uint32_t color)
{
    const int cell = 6;
    uint32_t a = PIX_A(color);
    int x, y;

    for (y = r->top; y < r->bottom; y += cell)
    {
        for (x = r->left; x < r->right; x += cell)
        {
            // Light and dark squares alternate
            uint32_t back = (((x - r->left) / cell + (y - r->top) / cell) & 1) ? 0xB0 : 0xFF;
            uint32_t rr = Pixel_Mul255(PIX_R(color), a) + Pixel_Mul255(back, 255 - a);
            uint32_t gg = Pixel_Mul255(PIX_G(color), a) + Pixel_Mul255(back, 255 - a);
            uint32_t bb = Pixel_Mul255(PIX_B(color), a) + Pixel_Mul255(back, 255 - a);
            RECT sq = { x, y, min(x + cell, r->right), min(y + cell, r->bottom) };
            HBRUSH brush = CreateSolidBrush(RGB(rr, gg, bb));
            FillRect(hdc, &sq, brush);
            DeleteObject(brush);
        }
    }
    FrameRect(hdc, r, (HBRUSH)GetStockObject(DKGRAY_BRUSH));
}

static void
Palette_OnPaint(Palette *p)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(p->hwnd, &ps);
    PaletteLayout lay;
    int i;

    Palette_GetLayout(p, &lay);

    // Secondary first, so the primary overlaps it
    Palette_DrawSwatch(hdc, &lay.secondary, p->settings->secondary);
    Palette_DrawSwatch(hdc, &lay.primary, p->settings->primary);

    for (i = 0; i < QUICK_COLS * QUICK_ROWS; i++)
    {
        RECT cell;
        HBRUSH brush = CreateSolidBrush(g_quickColors[i]);
        cell.left = lay.quick.left + (i % QUICK_COLS) * lay.cell;
        cell.top = lay.quick.top + (i / QUICK_COLS) * lay.cell;
        cell.right = cell.left + lay.cell;
        cell.bottom = cell.top + lay.cell;
        FillRect(hdc, &cell, brush);
        FrameRect(hdc, &cell, GetSysColorBrush(COLOR_3DSHADOW));
        DeleteObject(brush);
    }
    EndPaint(p->hwnd, &ps);
}

// Function: Palette_OnClick
// Works out what was clicked in the swatch and quick colour area.
// Left button changes the primary colour, right button the secondary.
static void
Palette_OnClick(Palette *p, int x, int y, BOOL rightButton)
{
    PaletteLayout lay;
    POINT pt = { x, y };

    Palette_GetLayout(p, &lay);

    if (PtInRect(&lay.primary, pt))
    {
        Palette_PickColor(p, TRUE);
    }
    else if (PtInRect(&lay.secondary, pt))
    {
        Palette_PickColor(p, FALSE);
    }
    else if (PtInRect(&lay.quick, pt))
    {
        int col = (x - lay.quick.left) / lay.cell;
        int row = (y - lay.quick.top) / lay.cell;
        COLORREF c = g_quickColors[row * QUICK_COLS + col];

        if (rightButton)
            p->settings->secondary = Pixel_FromColorRef(c, 255);
        else // Keep whatever alpha the slider is set to
            p->settings->primary = Pixel_FromColorRef(c, PIX_A(p->settings->primary));
        InvalidateRect(p->hwnd, NULL, FALSE);
    }
}

// Function: Palette_PickColor
// Shows the standard Windows colour dialog and stores the result.
static void
Palette_PickColor(Palette *p, BOOL primary)
{
    CHOOSECOLORW cc;
    uint32_t *target = primary ? &p->settings->primary : &p->settings->secondary;

    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = p->hwnd;
    cc.rgbResult = Pixel_ToColorRef(*target);   // Start from the current colour
    cc.lpCustColors = p->customColors;          // Windows remembers mixed colours here
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;

    if (ChooseColorW(&cc))
        *target = Pixel_FromColorRef(cc.rgbResult, PIX_A(*target));
    InvalidateRect(p->hwnd, NULL, FALSE);
}

static void
Palette_UpdateAlphaLabel(Palette *p)
{
    wchar_t text[32];
    StringCchPrintfW(text, ARRAYSIZE(text), L"Alpha: %u", (unsigned)PIX_A(p->settings->primary));
    SetWindowTextW(p->hAlphaLabel, text);
}

static LRESULT CALLBACK
Palette_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Palette *p = (Palette *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        p = (Palette *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        p->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)p);
        break;
    case WM_CREATE:
        return Palette_OnCreate(p) ? 0 : -1;
    case WM_NCDESTROY:
        free(p);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    case WM_SIZE:
        Palette_Layout(p);
        return 0;
    case WM_PAINT:
        Palette_OnPaint(p);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        Palette_OnClick(p, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), msg == WM_RBUTTONDOWN);
        return 0;
    case WM_HSCROLL:
        // The trackbar tells its parent about every movement with WM_HSCROLL
        if ((HWND)lParam == p->hAlpha)
        {
            uint32_t alpha = (uint32_t)SendMessageW(p->hAlpha, TBM_GETPOS, 0, 0);
            p->settings->primary = (p->settings->primary & 0x00FFFFFF) | (alpha << 24);
            Palette_UpdateAlphaLabel(p);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        // Static controls ask their parent what colours to use. We make the
        // label dark grey on the same colour as the palette.
        SetTextColor((HDC)wParam, RGB(60, 60, 60));
        SetBkColor((HDC)wParam, GetSysColor(COLOR_BTNFACE));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SWAP)
        {
            Palette_SwapColors(hwnd);
            return 0;
        }
        // Tool buttons tell their parent, which is us. We pass it up to the main window.
        SendMessageW(GetParent(hwnd), WM_COMMAND, wParam, lParam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
