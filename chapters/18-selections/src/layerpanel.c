/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 *
 * File: layerpanel.c
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <strsafe.h>
#include "layerpanel.h"
#include "blend.h"
#include "pixel.h"
#include "resource.h"
#include "uiutil.h"

#define PANEL_CLASS     L"DrawLiteLayerPanel"
#define LIST_CLASS      L"DrawLiteLayerList"

#define THUMB           36      // Thumbnail size at 96 DPI
#define ROW_HEIGHT      46      // Row height at 96 DPI

//----------------------------------------------------------------------------
// The list: a window that draws one row per layer
//----------------------------------------------------------------------------

typedef struct LayerList
{
    HWND hwnd;
    Document *doc;
    HFONT font;
    int scrollY;
} LayerList;

// Struct: LayerPanel
typedef struct LayerPanel
{
    HWND hwnd;
    Document *doc;
    HWND hList;
    HWND hOpacity;
    HWND hOpacityLabel;
    HWND hBlendLabel;
    HWND hBlend;
    HWND hButtons[5];
} LayerPanel;

static const int g_buttonIds[5] =
{
    IDM_LAYER_ADD, IDM_LAYER_DUPLICATE, IDM_LAYER_DELETE, IDM_LAYER_UP, IDM_LAYER_DOWN
};
static const wchar_t *g_buttonText[5] = { L"New", L"Copy", L"Del", L"Up", L"Down" };

static LRESULT CALLBACK LayerPanel_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK LayerList_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int
List_RowHeight(const LayerList *ll)
{
    return Ui_Scale(GetDpiForWindow(ll->hwnd), ROW_HEIGHT);
}

// Function: List_ClampScroll
// Keeps the list scrolled within its rows, and tells the scroll bar.
static void
List_UpdateScroll(LayerList *ll)
{
    RECT client;
    SCROLLINFO si;
    int total = ll->doc ? ll->doc->layerCount * List_RowHeight(ll) : 0;

    GetClientRect(ll->hwnd, &client);
    ll->scrollY = max(0, min(ll->scrollY, total - client.bottom));

    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = max(total - 1, 0);
    si.nPage = client.bottom;
    si.nPos = ll->scrollY;
    SetScrollInfo(ll->hwnd, SB_VERT, &si, TRUE);
}

// Function: List_LayerAtRow
// The top row shows the top layer, so rows count down while layer numbers count up.
static int
List_LayerAtRow(const LayerList *ll, int row)
{
    return ll->doc->layerCount - 1 - row;
}

// Function: List_EnsureVisible
// Scrolls so that the active layer's row can be seen.
static void
List_EnsureVisible(LayerList *ll)
{
    RECT client;
    int rowH = List_RowHeight(ll);
    int top, bottom;

    if (!ll->doc)
        return;
    GetClientRect(ll->hwnd, &client);
    top = (ll->doc->layerCount - 1 - ll->doc->active) * rowH;
    bottom = top + rowH;
    if (top < ll->scrollY)
        ll->scrollY = top;
    else if (bottom > ll->scrollY + client.bottom)
        ll->scrollY = bottom - client.bottom;
    List_UpdateScroll(ll);
}

// Function: List_DrawThumbnail
// Draws a tiny picture of a layer. We squash it down ourselves by picking
// pixels (no smoothing), over a little chequerboard so see-through shows.
static void
List_DrawThumbnail(HDC hdc, const RECT *box, const Layer *layer)
{
    int bw = box->right - box->left, bh = box->bottom - box->top;
    const Surface *s = layer->surface;
    uint32_t *buf = (uint32_t *)malloc((size_t)bw * bh * sizeof(uint32_t));
    BITMAPINFO bi;
    float scale;
    int tw, th, ox, oy, x, y;

    if (!buf)
        return;

    // Fit the whole picture in the box, keeping its shape
    scale = (float)bw / s->width;
    if ((float)bh / s->height < scale)
        scale = (float)bh / s->height;
    tw = max(1, (int)(s->width * scale));
    th = max(1, (int)(s->height * scale));
    ox = (bw - tw) / 2;
    oy = (bh - th) / 2;

    for (y = 0; y < bh; y++)
    {
        for (x = 0; x < bw; x++)
        {
            uint32_t check = (((x / 4) + (y / 4)) & 1) ? 0xFFC0C0C0 : 0xFFFFFFFF;
            uint32_t p = 0;

            if (x >= ox && x < ox + tw && y >= oy && y < oy + th)
            {
                int sx = min(s->width - 1, (int)((x - ox) / scale));
                int sy = min(s->height - 1, (int)((y - oy) / scale));
                p = s->pixels[(size_t)sy * s->width + sx];
            }
            else
                check = 0xFF808080;     // Outside the picture
            buf[(size_t)y * bw + x] = Pixel_Over(p, check);
        }
    }

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bw;
    bi.bmiHeader.biHeight = -bh;        // Negative height means the rows go top to bottom
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, box->left, box->top, (DWORD)bw, (DWORD)bh, 0, 0, 0, (UINT)bh, buf, &bi, DIB_RGB_COLORS);
    free(buf);
    FrameRect(hdc, box, (HBRUSH)GetStockObject(DKGRAY_BRUSH));
}

// Function: List_RowRects
// Where the eye box, the thumbnail and the text go in a row.
static void
List_RowRects(const LayerList *ll, int rowTop, RECT *eye, RECT *thumb, RECT *text)
{
    UINT dpi = GetDpiForWindow(ll->hwnd);
    int rowH = Ui_Scale(dpi, ROW_HEIGHT);
    int margin = Ui_Scale(dpi, 4);
    int t = Ui_Scale(dpi, THUMB);
    int eyeSize = Ui_Scale(dpi, 16);
    RECT client;

    GetClientRect(ll->hwnd, &client);
    SetRect(eye, margin, rowTop + (rowH - eyeSize) / 2, margin + eyeSize, rowTop + (rowH - eyeSize) / 2 + eyeSize);
    SetRect(thumb, eye->right + margin, rowTop + (rowH - t) / 2, eye->right + margin + t, rowTop + (rowH - t) / 2 + t);
    SetRect(text, thumb->right + margin * 2, rowTop, client.right - margin, rowTop + rowH);
}

static void
List_OnPaint(LayerList *ll)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(ll->hwnd, &ps);
    RECT client;
    HFONT oldFont;
    int rowH = List_RowHeight(ll);
    int row;

    GetClientRect(ll->hwnd, &client);
    FillRect(hdc, &client, GetSysColorBrush(COLOR_WINDOW));
    oldFont = (HFONT)SelectObject(hdc, ll->font ? ll->font : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);

    if (ll->doc)
    {
        for (row = 0; row < ll->doc->layerCount; row++)
        {
            int rowTop = row * rowH - ll->scrollY;
            int index = List_LayerAtRow(ll, row);
            const Layer *layer = ll->doc->layers[index];
            BOOL active = index == ll->doc->active;
            RECT full, eye, thumb, text, line1, line2;
            wchar_t info[64];
            COLORREF fg;

            if (rowTop + rowH < ps.rcPaint.top || rowTop > ps.rcPaint.bottom)
                continue;

            SetRect(&full, 0, rowTop, client.right, rowTop + rowH);
            FillRect(hdc, &full, GetSysColorBrush(active ? COLOR_HIGHLIGHT : COLOR_WINDOW));
            fg = GetSysColor(active ? COLOR_HIGHLIGHTTEXT : (layer->visible ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
            SetTextColor(hdc, fg);

            List_RowRects(ll, rowTop, &eye, &thumb, &text);
            DrawFrameControl(hdc, &eye, DFC_BUTTON, DFCS_BUTTONCHECK | (layer->visible ? DFCS_CHECKED : 0));
            List_DrawThumbnail(hdc, &thumb, layer);

            line1 = text;
            line1.bottom = rowTop + rowH / 2;
            line2 = text;
            line2.top = line1.bottom;
            DrawTextW(hdc, layer->name, -1, &line1, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            StringCchPrintfW(info, ARRAYSIZE(info), L"%d%%  %s", (layer->opacity * 100 + 127) / 255, Blend_Name(layer->blend));
            DrawTextW(hdc, info, -1, &line2, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
    }
    SelectObject(hdc, oldFont);
    EndPaint(ll->hwnd, &ps);
}

static void
List_OnClick(LayerList *ll, int x, int y, BOOL doubleClick)
{
    HWND parent = GetParent(ll->hwnd);
    int rowH = List_RowHeight(ll);
    int row, index;
    RECT eye, thumb, text;

    if (!ll->doc || y < 0)
        return;
    row = (y + ll->scrollY) / rowH;
    if (row >= ll->doc->layerCount)
        return;
    index = List_LayerAtRow(ll, row);
    List_RowRects(ll, row * rowH - ll->scrollY, &eye, &thumb, &text);

    if (!doubleClick && x >= eye.left && x < eye.right)
    {
        SendMessageW(parent, WMU_LAYER_VISIBLE, (WPARAM)index, 0);
        return;
    }
    if (index != ll->doc->active)
        SendMessageW(parent, WMU_LAYER_SELECT, (WPARAM)index, 0);
    if (doubleClick && x >= text.left)
        SendMessageW(parent, WMU_LAYER_RENAME, (WPARAM)index, 0);
}

static LRESULT CALLBACK
LayerList_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LayerList *ll = (LayerList *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        ll = (LayerList *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        ll->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ll);
        break;
    case WM_NCDESTROY:
        free(ll);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    case WM_SETFONT:
        ll->font = (HFONT)wParam;       // We do not own it. The main window does.
        if (LOWORD(lParam))
            InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_SIZE:
        List_UpdateScroll(ll);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        List_OnPaint(ll);
        return 0;
    case WM_LBUTTONDOWN:
        List_OnClick(ll, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), FALSE);
        return 0;
    case WM_LBUTTONDBLCLK:
        List_OnClick(ll, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), TRUE);
        return 0;
    case WM_MOUSEWHEEL:
        ll->scrollY -= GET_WHEEL_DELTA_WPARAM(wParam) * List_RowHeight(ll) / WHEEL_DELTA;
        List_UpdateScroll(ll);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_VSCROLL:
    {
        SCROLLINFO si;
        int pos;

        ZeroMemory(&si, sizeof(si));
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        pos = si.nPos;
        switch (LOWORD(wParam))
        {
        case SB_LINEUP:     pos -= List_RowHeight(ll) / 2; break;
        case SB_LINEDOWN:   pos += List_RowHeight(ll) / 2; break;
        case SB_PAGEUP:     pos -= (int)si.nPage; break;
        case SB_PAGEDOWN:   pos += (int)si.nPage; break;
        case SB_THUMBTRACK: pos = si.nTrackPos; break;
        default:            return 0;
        }
        ll->scrollY = pos;
        List_UpdateScroll(ll);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//----------------------------------------------------------------------------
// The panel: the list plus the controls under it
//----------------------------------------------------------------------------

static BOOL
LayerPanel_RegisterClasses(HINSTANCE hInstance)
{
    static BOOL registered = FALSE;
    WNDCLASSEXW wcx;

    if (registered)
        return TRUE;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(wcx);
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);

    wcx.lpfnWndProc = LayerPanel_WndProc;
    wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcx.lpszClassName = PANEL_CLASS;
    if (!RegisterClassExW(&wcx))
        return FALSE;

    wcx.lpfnWndProc = LayerList_WndProc;
    wcx.style = CS_DBLCLKS;                 // We want WM_LBUTTONDBLCLK
    wcx.hbrBackground = NULL;
    wcx.lpszClassName = LIST_CLASS;
    if (!RegisterClassExW(&wcx))
        return FALSE;

    registered = TRUE;
    return TRUE;
}

HWND
LayerPanel_Create(HWND parent, HINSTANCE hInstance, int id)
{
    LayerPanel *p;
    HWND hwnd;

    if (!LayerPanel_RegisterClasses(hInstance))
        return NULL;
    p = (LayerPanel *)calloc(1, sizeof(LayerPanel));
    if (!p)
        return NULL;

    hwnd = CreateWindowExW(0, PANEL_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, hInstance, p);
    if (!hwnd)
        free(p);
    return hwnd;
}

static BOOL
LayerPanel_OnCreate(LayerPanel *p)
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(p->hwnd, GWLP_HINSTANCE);
    LayerList *ll = (LayerList *)calloc(1, sizeof(LayerList));
    int i;

    if (!ll)
        return FALSE;
    p->hList = CreateWindowExW(WS_EX_CLIENTEDGE, LIST_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL,
        0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_LAYER_LIST, hInstance, ll);
    if (!p->hList)
    {
        free(ll);
        return FALSE;
    }
    for (i = 0; i < 5; i++)
        p->hButtons[i] = CreateWindowExW(0, L"BUTTON", g_buttonText[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)g_buttonIds[i], hInstance, NULL);

    p->hOpacityLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_LAYER_OPACITY_LABEL, hInstance, NULL);
    p->hOpacity = CreateWindowExW(0, TRACKBAR_CLASSW, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_LAYER_OPACITY, hInstance, NULL);
    p->hBlendLabel = CreateWindowExW(0, L"STATIC", L"Blend mode:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, p->hwnd, NULL, hInstance, NULL);
    p->hBlend = CreateWindowExW(0, L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_LAYER_BLEND, hInstance, NULL);
    if (!p->hOpacity || !p->hBlend)
        return FALSE;

    SendMessageW(p->hOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    for (i = 0; i < BLEND_COUNT; i++)
        SendMessageW(p->hBlend, CB_ADDSTRING, 0, (LPARAM)Blend_Name((BlendMode)i));
    SendMessageW(p->hBlend, CB_SETCURSEL, 0, 0);
    return TRUE;
}

static void
LayerPanel_Layout(LayerPanel *p)
{
    RECT rc;
    UINT dpi = GetDpiForWindow(p->hwnd);
    int m = Ui_Scale(dpi, 6);
    int rowH = Ui_Scale(dpi, 24);
    int labelH = Ui_Scale(dpi, 18);
    int barH = Ui_Scale(dpi, 28);
    int comboH = Ui_Scale(dpi, 24);
    int w, bw, y, i;

    GetClientRect(p->hwnd, &rc);
    w = rc.right - 2 * m;

    // Build up from the bottom
    y = rc.bottom - m - comboH;
    MoveWindow(p->hBlend, m, y, w, Ui_Scale(dpi, 200), TRUE);     // The drop down list needs room below it
    y -= labelH;
    MoveWindow(p->hBlendLabel, m, y, w, labelH, TRUE);
    y -= barH;
    MoveWindow(p->hOpacity, m, y, w, barH, TRUE);
    y -= labelH;
    MoveWindow(p->hOpacityLabel, m, y, w, labelH, TRUE);
    y -= rowH + m;
    bw = (w - 4 * (m / 2)) / 5;
    for (i = 0; i < 5; i++)
        MoveWindow(p->hButtons[i], m + i * (bw + m / 2), y, bw, rowH, TRUE);
    y -= m;

    MoveWindow(p->hList, m, m, w, max(y - m, 0), TRUE);
}

void
LayerPanel_SetDocument(HWND panel, Document *doc)
{
    LayerPanel *p = (LayerPanel *)GetWindowLongPtrW(panel, GWLP_USERDATA);
    LayerList *ll = (LayerList *)GetWindowLongPtrW(p->hList, GWLP_USERDATA);

    p->doc = doc;
    ll->doc = doc;
    ll->scrollY = 0;
    LayerPanel_Refresh(panel);
}

void
LayerPanel_Refresh(HWND panel)
{
    LayerPanel *p = (LayerPanel *)GetWindowLongPtrW(panel, GWLP_USERDATA);
    LayerList *ll = (LayerList *)GetWindowLongPtrW(p->hList, GWLP_USERDATA);
    wchar_t text[48];

    if (p->doc)
    {
        const Layer *layer = Doc_ActiveLayer(p->doc);
        SendMessageW(p->hOpacity, TBM_SETPOS, TRUE, layer->opacity);
        StringCchPrintfW(text, ARRAYSIZE(text), L"Opacity: %d%%", (layer->opacity * 100 + 127) / 255);
        SetWindowTextW(p->hOpacityLabel, text);
        SendMessageW(p->hBlend, CB_SETCURSEL, (WPARAM)layer->blend, 0);
    }
    List_EnsureVisible(ll);
    InvalidateRect(p->hList, NULL, FALSE);
}

static LRESULT CALLBACK
LayerPanel_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LayerPanel *p = (LayerPanel *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        p = (LayerPanel *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        p->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)p);
        break;
    case WM_CREATE:
        return LayerPanel_OnCreate(p) ? 0 : -1;
    case WM_NCDESTROY:
        free(p);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    case WM_SIZE:
        LayerPanel_Layout(p);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_LAYER_BLEND)
        {
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                LRESULT sel = SendMessageW(p->hBlend, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR)
                    SendMessageW(GetParent(hwnd), WMU_LAYER_BLEND, (WPARAM)sel, 0);
            }
            return 0;
        }
        // The buttons use the same ids as the Layer menu. Pass them straight up.
        SendMessageW(GetParent(hwnd), WM_COMMAND, wParam, lParam);
        return 0;
    case WM_HSCROLL:
        // The opacity slider. TB_THUMBTRACK means "still dragging". Anything else is final.
        if ((HWND)lParam == p->hOpacity)
        {
            int value = (int)SendMessageW(p->hOpacity, TBM_GETPOS, 0, 0);
            wchar_t text[48];

            StringCchPrintfW(text, ARRAYSIZE(text), L"Opacity: %d%%", (value * 100 + 127) / 255);
            SetWindowTextW(p->hOpacityLabel, text);
            SendMessageW(GetParent(hwnd), WMU_LAYER_OPACITY, (WPARAM)value, LOWORD(wParam) != TB_THUMBTRACK);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
