/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: texttool.c
 */

#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <string.h>
#include "texttool.h"
#include "pixel.h"

#define SUBCLASS_ID     1

static UINT g_serial = 0;

// Function: Text_SizeToFit
// Makes the edit control just big enough for what has been typed, so it
// grows instead of scrolling. We call it once at the start; see the exercise.
static void
Text_SizeToFit(TextBox *tb)
{
    HDC dc = GetDC(tb->hwnd);
    HFONT old = (HFONT)SelectObject(dc, tb->font);
    int len = GetWindowTextLengthW(tb->hwnd);
    wchar_t *text = (wchar_t *)malloc((size_t)(len + 2) * sizeof(wchar_t));
    RECT r = { 0, 0, 0, 0 };
    TEXTMETRICW tm;
    int w, h;

    GetTextMetricsW(dc, &tm);
    if (text)
    {
        GetWindowTextW(tb->hwnd, text, len + 1);
        // A trailing new line should show as an empty last line, which DrawText ignores. Add a space.
        if (len > 0 && text[len - 1] == L'\n')
        {
            text[len] = L' ';
            text[len + 1] = L'\0';
        }
        DrawTextW(dc, text, -1, &r, DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS | DT_LEFT | DT_TOP);
        free(text);
    }
    SelectObject(dc, old);
    ReleaseDC(tb->hwnd, dc);

    // Room for the caret and a little breathing space
    w = max(r.right, tm.tmAveCharWidth * 4) + tm.tmAveCharWidth * 2;
    h = max(r.bottom, tm.tmHeight);
    SetWindowPos(tb->hwnd, NULL, 0, 0, w + 2, h + 2, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Function: Text_SubclassProc
// "Subclassing" means putting our own window procedure in front of the control's,
// so that we see its messages first. We use it to catch the keys that mean
// "finished" and to notice when the control loses the keyboard.
static LRESULT CALLBACK
Text_SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR refData)
{
    TextBox *tb = (TextBox *)refData;

    UNREFERENCED_PARAMETER(id);
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            // Posted, not sent: we must not destroy the window from inside its own message
            PostMessageW(tb->canvas, WMU_TEXT_DONE, tb->serial, FALSE);
            return 0;
        }
        if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            PostMessageW(tb->canvas, WMU_TEXT_DONE, tb->serial, TRUE);
            return 0;
        }
        break;
    case WM_CHAR:
        // Ctrl+Enter makes a line feed character, which would beep. We handled it above.
        if (wParam == '\n')
            return 0;
        break;
    case WM_KILLFOCUS:
        // Clicking somewhere else finishes the text
        PostMessageW(tb->canvas, WMU_TEXT_DONE, tb->serial, TRUE);
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, Text_SubclassProc, SUBCLASS_ID);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

TextBox *
Text_Create(HWND canvas, const LOGFONTW *logFont, uint32_t color, int zoom,
    int winX, int winY, int imageX, int imageY)
{
    TextBox *tb = (TextBox *)calloc(1, sizeof(TextBox));
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(canvas, GWLP_HINSTANCE);
    LOGFONTW shown;
    int lum;

    if (!tb)
        return NULL;
    tb->canvas = canvas;
    tb->logFont = *logFont;
    tb->color = color;
    tb->zoom = zoom;
    tb->imageX = imageX;
    tb->imageY = imageY;
    tb->serial = ++g_serial;
    tb->dpi = GetDpiForWindow(canvas);

    // The font we show while editing is the final font, scaled by the zoom
    shown = *logFont;
    shown.lfHeight = -max(6, abs(logFont->lfHeight) * zoom / 100);
    tb->font = CreateFontIndirectW(&shown);

    // An edit control cannot be see-through. So choose a background that
    // will not hide the text: light text gets a dark background and the other way round.
    tb->textColor = RGB(PIX_R(color), PIX_G(color), PIX_B(color));
    lum = (int)(PIX_R(color) * 30 + PIX_G(color) * 59 + PIX_B(color) * 11) / 100;
    tb->backColor = lum > 140 ? RGB(60, 60, 60) : RGB(255, 255, 255);
    tb->background = CreateSolidBrush(tb->backColor);

    tb->hwnd = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN,
        winX - 1, winY - 1, 40, 40, canvas, (HMENU)(INT_PTR)1000, hInstance, NULL);
    if (!tb->hwnd || !tb->font || !tb->background)
    {
        Text_Destroy(tb);
        return NULL;
    }
    SendMessageW(tb->hwnd, WM_SETFONT, (WPARAM)tb->font, TRUE);
    SendMessageW(tb->hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    SetWindowSubclass(tb->hwnd, Text_SubclassProc, SUBCLASS_ID, (DWORD_PTR)tb);
    Text_SizeToFit(tb);
    SetFocus(tb->hwnd);
    return tb;
}

void
Text_Destroy(TextBox *tb)
{
    if (!tb)
        return;
    if (tb->hwnd)
        DestroyWindow(tb->hwnd);
    if (tb->font)
        DeleteObject(tb->font);
    if (tb->background)
        DeleteObject(tb->background);
    free(tb);
}

wchar_t *
Text_GetString(const TextBox *tb)
{
    int len = GetWindowTextLengthW(tb->hwnd);
    wchar_t *text = (wchar_t *)calloc((size_t)len + 1, sizeof(wchar_t));
    if (text)
        GetWindowTextW(tb->hwnd, text, len + 1);
    return text;
}

uint8_t *
Text_Rasterize(const LOGFONTW *logFont, const wchar_t *text, int *w, int *h)
{
    static const UINT flags = DT_NOPREFIX | DT_EXPANDTABS | DT_LEFT | DT_TOP;
    HDC dc;
    LOGFONTW lf = *logFont;
    HFONT font, oldFont;
    HBITMAP dib, oldBitmap;
    BITMAPINFO bi;
    void *bits = NULL;
    RECT r = { 0, 0, 0, 0 };
    TEXTMETRICW tm;
    uint8_t *mask = NULL;
    int width, height, i;

    if (!text[0])
        return NULL;

    dc = CreateCompatibleDC(NULL);
    // GRAYSCALE anti-aliasing: ClearType colours the edges red and blue, which
    // is no use to us. We want a plain "how much ink" number for each pixel.
    lf.lfQuality = ANTIALIASED_QUALITY;
    font = CreateFontIndirectW(&lf);
    oldFont = (HFONT)SelectObject(dc, font);

    // First measure, then draw
    DrawTextW(dc, text, -1, &r, DT_CALCRECT | (int)flags);
    GetTextMetricsW(dc, &tm);
    // Italic letters can hang over the edge of the measured box, so add some room
    width = r.right + tm.tmHeight / 4 + 2;
    height = r.bottom + 2;

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (dib && bits)
    {
        oldBitmap = (HBITMAP)SelectObject(dc, dib);
        memset(bits, 0, (size_t)width * height * 4);     // Black paper
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));            // White ink
        DrawTextW(dc, text, -1, &r, flags);
        GdiFlush();     // Make sure GDI has finished drawing before we read the memory

        mask = (uint8_t *)malloc((size_t)width * height);
        if (mask)
        {
            const uint32_t *px = (const uint32_t *)bits;
            // White ink on black paper: the brightness of a pixel is how much ink is in it
            for (i = 0; i < width * height; i++)
                mask[i] = (uint8_t)(px[i] & 0xFF);
            *w = width;
            *h = height;
        }
        SelectObject(dc, oldBitmap);
        DeleteObject(dib);
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
    return mask;
}
