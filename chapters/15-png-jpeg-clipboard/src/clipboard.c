/* DrawLite - Win32 Tutorial
 * Chapter 15 - PNG, JPEG and the clipboard
 *
 * File: clipboard.c
 *
 * The clipboard holds the same data in several formats at once. The program
 * that copies offers every format it can make, and the program that pastes
 * takes the best one it understands. A format is just a number: CF_DIB and
 * friends are built in, and anything else (like "PNG") we register by name.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "clipboard.h"
#include "bmp.h"
#include "imageio.h"

static UINT
Clipboard_PngFormat(void)
{
    // RegisterClipboardFormat gives the same number to everyone who asks for the same name
    static UINT format = 0;
    if (!format)
        format = RegisterClipboardFormatW(L"PNG");
    return format;
}

// Function: Clipboard_Put
// Copies a block of memory into a new global memory block and hands it to the
// clipboard. The clipboard owns the block after a successful SetClipboardData.
static BOOL
Clipboard_Put(UINT format, const void *data, size_t size)
{
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
    void *dst;

    if (!mem)
        return FALSE;
    dst = GlobalLock(mem);
    if (!dst)
    {
        GlobalFree(mem);
        return FALSE;
    }
    memcpy(dst, data, size);
    GlobalUnlock(mem);

    if (!SetClipboardData(format, mem))
    {
        GlobalFree(mem);    // It did not take it, so it is still ours
        return FALSE;
    }
    return TRUE;
}

BOOL
Clipboard_CopyImage(HWND owner, const Surface *s)
{
    uint8_t *png, *bmp;
    size_t pngSize = 0, bmpSize = 0;
    BOOL ok = FALSE;

    // Do the slow encoding before we open the clipboard. Nobody else can use
    // it while we have it open.
    png = ImageIO_EncodePng(s, &pngSize);
    bmp = Bmp_Encode(s, &bmpSize);
    if (!bmp)
    {
        free(png);
        return FALSE;
    }

    if (OpenClipboard(owner))
    {
        EmptyClipboard();   // Throw away whatever was there. We now own the clipboard.
        ok = TRUE;
        if (png)
            Clipboard_Put(Clipboard_PngFormat(), png, pngSize);

        // A .bmp file is a file header followed by exactly what the clipboard
        // wants, so we just skip the header. If the picture has transparency
        // the data starts with a V5 header, which has its own format name.
        {
            const BITMAPINFOHEADER *ih = (const BITMAPINFOHEADER *)(bmp + sizeof(BITMAPFILEHEADER));
            UINT format = ih->biSize >= sizeof(BITMAPV5HEADER) ? CF_DIBV5 : CF_DIB;
            ok = Clipboard_Put(format, bmp + sizeof(BITMAPFILEHEADER), bmpSize - sizeof(BITMAPFILEHEADER));
        }
        CloseClipboard();
    }
    free(png);
    free(bmp);
    return ok;
}

BOOL
Clipboard_HasImage(void)
{
    return IsClipboardFormatAvailable(Clipboard_PngFormat())
        || IsClipboardFormatAvailable(CF_DIBV5)
        || IsClipboardFormatAvailable(CF_DIB);
}

// Function: Clipboard_DibToSurface
// Clipboard DIBs have no file header. We put one on the front and use the
// bitmap reader we already have.
static Surface *
Clipboard_DibToSurface(const uint8_t *dib, size_t size, wchar_t *error, size_t errorLen)
{
    const BITMAPINFOHEADER *ih = (const BITMAPINFOHEADER *)dib;
    size_t headerLen = sizeof(BITMAPFILEHEADER);
    size_t pixelsAt;
    uint8_t *file;
    BITMAPFILEHEADER fh;
    Surface *s;

    if (size < sizeof(BITMAPINFOHEADER) || ih->biSize < sizeof(BITMAPINFOHEADER))
    {
        StringCchCopyW(error, errorLen, L"The clipboard picture is in a format we do not understand.");
        return NULL;
    }

    // Where do the pixels start? After the header, then the colour table if
    // there is one, then (for a plain 40 byte header only) the three colour masks.
    pixelsAt = ih->biSize;
    if (ih->biBitCount <= 8)
        pixelsAt += (size_t)(ih->biClrUsed ? ih->biClrUsed : (1u << ih->biBitCount)) * 4;
    if (ih->biCompression == BI_BITFIELDS && ih->biSize == sizeof(BITMAPINFOHEADER))
        pixelsAt += 12;

    file = (uint8_t *)malloc(headerLen + size);
    if (!file)
    {
        StringCchCopyW(error, errorLen, L"Not enough memory.");
        return NULL;
    }
    ZeroMemory(&fh, sizeof(fh));
    fh.bfType = 0x4D42;
    fh.bfSize = (DWORD)(headerLen + size);
    fh.bfOffBits = (DWORD)(headerLen + pixelsAt);
    memcpy(file, &fh, sizeof(fh));
    memcpy(file + headerLen, dib, size);

    s = Bmp_Decode(file, headerLen + size, error, errorLen);
    free(file);
    return s;
}

Surface *
Clipboard_PasteImage(HWND owner, wchar_t *error, size_t errorLen)
{
    Surface *s = NULL;
    UINT formats[3];
    int i;

    formats[0] = Clipboard_PngFormat();     // Best: has transparency
    formats[1] = CF_DIBV5;
    formats[2] = CF_DIB;

    if (!OpenClipboard(owner))
    {
        StringCchCopyW(error, errorLen, L"The clipboard is in use by another program.");
        return NULL;
    }
    for (i = 0; i < 3 && !s; i++)
    {
        HGLOBAL mem;
        const uint8_t *data;

        if (!IsClipboardFormatAvailable(formats[i]))
            continue;
        mem = GetClipboardData(formats[i]);     // Still owned by the clipboard. Do not free.
        if (!mem)
            continue;
        data = (const uint8_t *)GlobalLock(mem);
        if (!data)
            continue;
        if (i == 0)
            s = ImageIO_LoadMemory(data, GlobalSize(mem), error, errorLen);
        else
            s = Clipboard_DibToSurface(data, GlobalSize(mem), error, errorLen);
        GlobalUnlock(mem);
    }
    CloseClipboard();

    if (!s && error[0] == L'\0')
        StringCchCopyW(error, errorLen, L"There is no picture on the clipboard.");
    return s;
}
