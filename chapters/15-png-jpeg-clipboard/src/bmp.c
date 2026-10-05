/* DrawLite - Win32 Tutorial
 * Chapter 15 - PNG, JPEG and the clipboard
 *
 * File: bmp.c
 *
 * A .bmp file is laid out like this:
 *
 *   BITMAPFILEHEADER   14 bytes  "BM", file size, where the pixels start
 *   BITMAPINFOHEADER   40 bytes  (or a bigger version) width, height, bits per pixel...
 *   colour table       only for 8 bits per pixel or fewer
 *   pixels             rows padded to a multiple of 4 bytes, usually bottom row first
 *
 * All numbers are little endian, which is what we have, so we can use a
 * struct. Windows already defines the structs: see <wingdi.h>.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "bmp.h"
#include "pixel.h"

static void
Bmp_Fail(wchar_t *error, size_t errorLen, const wchar_t *message)
{
    if (error && errorLen)
        StringCchCopyW(error, errorLen, message);
}

// Function: Bmp_ReadFile
// Reads a whole file into memory using the Win32 file functions.
// Free the result with free().
static uint8_t *
Bmp_ReadFile(const wchar_t *path, size_t *size, wchar_t *error, size_t errorLen)
{
    HANDLE file;
    LARGE_INTEGER length;
    uint8_t *data;
    DWORD got = 0;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        Bmp_Fail(error, errorLen, L"The file could not be opened.");
        return NULL;
    }
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 26 || length.QuadPart > 0x7FFFFFFF)
    {
        CloseHandle(file);
        Bmp_Fail(error, errorLen, L"This does not look like a bitmap file.");
        return NULL;
    }
    data = (uint8_t *)malloc((size_t)length.QuadPart);
    if (!data || !ReadFile(file, data, (DWORD)length.QuadPart, &got, NULL) || got != length.QuadPart)
    {
        free(data);
        CloseHandle(file);
        Bmp_Fail(error, errorLen, L"The file could not be read.");
        return NULL;
    }
    CloseHandle(file);
    *size = (size_t)length.QuadPart;
    return data;
}

// Function: Bmp_MaskInfo
// For a colour mask like 0x00FF0000, finds how far to shift right (16)
// and how many bits it has (8).
static void
Bmp_MaskInfo(uint32_t mask, int *shift, int *bits)
{
    *shift = 0;
    *bits = 0;
    if (!mask)
        return;
    while (!(mask & 1))
    {
        mask >>= 1;
        (*shift)++;
    }
    while (mask & 1)
    {
        mask >>= 1;
        (*bits)++;
    }
}

// Function: Bmp_Channel
// Pulls one channel out of a pixel and scales it up to 0..255.
static uint32_t
Bmp_Channel(uint32_t value, uint32_t mask, int shift, int bits)
{
    uint32_t v;
    if (!mask || bits == 0)
        return 0;
    v = (value & mask) >> shift;
    if (bits == 8)
        return v;
    return v * 255 / ((1u << bits) - 1);
}

Surface *
Bmp_Decode(const uint8_t *data, size_t size, wchar_t *error, size_t errorLen)
{
    const BITMAPFILEHEADER *fh = (const BITMAPFILEHEADER *)data;
    const BITMAPINFOHEADER *ih = (const BITMAPINFOHEADER *)(data + sizeof(BITMAPFILEHEADER));
    uint32_t headerSize, offset, colorsUsed = 0;
    int width, height, bpp;
    BOOL topDown;
    const uint8_t *palette;
    uint32_t paletteCount = 0;
    uint32_t masks[4] = { 0, 0, 0, 0 };
    int shifts[4] = { 0 }, bits[4] = { 0 };
    size_t stride;
    Surface *s;
    BOOL sawAlpha = FALSE;
    int x, y;

    if (size < sizeof(BITMAPFILEHEADER) + 12 || fh->bfType != 0x4D42) // "BM"
    {
        Bmp_Fail(error, errorLen, L"This is not a bitmap file.");
        return NULL;
    }
    headerSize = ih->biSize;
    if (headerSize < 40 || sizeof(BITMAPFILEHEADER) + headerSize > size)
    {
        Bmp_Fail(error, errorLen, L"This kind of bitmap (an old OS/2 one, perhaps) is not supported.");
        return NULL;
    }

    width = ih->biWidth;
    height = ih->biHeight;
    topDown = height < 0;
    if (topDown)
        height = -height;
    bpp = ih->biBitCount;
    offset = fh->bfOffBits;
    colorsUsed = ih->biClrUsed;

    if (width <= 0 || height <= 0 || width > 32768 || height > 32768)
    {
        Bmp_Fail(error, errorLen, L"The bitmap has a strange size.");
        return NULL;
    }
    if (ih->biCompression != BI_RGB && ih->biCompression != BI_BITFIELDS)
    {
        Bmp_Fail(error, errorLen, L"Compressed bitmaps are not supported.");
        return NULL;
    }
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)
    {
        Bmp_Fail(error, errorLen, L"This number of bits per pixel is not supported.");
        return NULL;
    }

    // The colour table follows the header
    palette = data + sizeof(BITMAPFILEHEADER) + headerSize;
    if (bpp <= 8)
    {
        paletteCount = colorsUsed ? colorsUsed : (1u << bpp);
        if (paletteCount > (1u << bpp))
            paletteCount = 1u << bpp;
        if ((size_t)(palette - data) + (size_t)paletteCount * 4 > size)
        {
            Bmp_Fail(error, errorLen, L"The bitmap is damaged (colour table).");
            return NULL;
        }
    }

    // 32 bit files may say which bits are red, green, blue and alpha
    if (bpp == 32)
    {
        masks[0] = 0x00FF0000; masks[1] = 0x0000FF00; masks[2] = 0x000000FF; masks[3] = 0;
        if (ih->biCompression == BI_BITFIELDS)
        {
            // The masks come right after the first 40 bytes of the header. With a
            // plain 40 byte header they sit between the header and the pixels.
            if (sizeof(BITMAPFILEHEADER) + 40 + 12 <= size)
            {
                const uint32_t *m = (const uint32_t *)(data + sizeof(BITMAPFILEHEADER) + 40);
                masks[0] = m[0]; masks[1] = m[1]; masks[2] = m[2];
                if (headerSize >= 56)
                    masks[3] = m[3];
            }
        }
        for (x = 0; x < 4; x++)
            Bmp_MaskInfo(masks[x], &shifts[x], &bits[x]);
    }

    // Each row is padded to a multiple of 4 bytes
    stride = (((size_t)width * bpp + 31) / 32) * 4;
    if (offset >= size || stride * (size_t)height > size - offset)
    {
        Bmp_Fail(error, errorLen, L"The bitmap is damaged (not enough pixels).");
        return NULL;
    }

    s = Surface_Create(width, height);
    if (!s)
    {
        Bmp_Fail(error, errorLen, L"There is not enough memory for an image this big.");
        return NULL;
    }

    for (y = 0; y < height; y++)
    {
        // Normally the first row in the file is the BOTTOM row of the picture
        const uint8_t *row = data + offset + stride * (size_t)(topDown ? y : height - 1 - y);
        uint32_t *out = s->pixels + (size_t)y * width;

        for (x = 0; x < width; x++)
        {
            switch (bpp)
            {
            case 1:
            case 4:
            case 8:
            {
                // Several pixels are packed in each byte, leftmost pixel in the top bits
                int perByte = 8 / bpp;
                uint32_t index = (row[x / perByte] >> (8 - bpp * (x % perByte + 1))) & ((1u << bpp) - 1);
                const uint8_t *c = palette + (size_t)(index < paletteCount ? index : 0) * 4; // B, G, R, unused
                out[x] = PIX_MAKE(255, c[2], c[1], c[0]);
                break;
            }
            case 24:
                // Blue comes first in the file
                out[x] = PIX_MAKE(255, row[x * 3 + 2], row[x * 3 + 1], row[x * 3]);
                break;
            case 32:
            {
                uint32_t v;
                uint32_t r, g, b, a;
                memcpy(&v, row + x * 4, 4);
                r = Bmp_Channel(v, masks[0], shifts[0], bits[0]);
                g = Bmp_Channel(v, masks[1], shifts[1], bits[1]);
                b = Bmp_Channel(v, masks[2], shifts[2], bits[2]);
                a = masks[3] ? Bmp_Channel(v, masks[3], shifts[3], bits[3]) : 255;
                if (a != 0)
                    sawAlpha = TRUE;
                // The file has normal colours. We want premultiplied.
                out[x] = Pixel_Premultiply(PIX_MAKE(a, r, g, b));
                break;
            }
            }
        }
    }

    // Plenty of programs write 32 bit files with the alpha byte left at zero,
    // meaning "I did not use it". If we took that literally the picture would
    // be invisible. So if no pixel has any alpha at all, make them all opaque.
    if (bpp == 32 && masks[3] && !sawAlpha)
        for (y = 0; y < width * height; y++)
            s->pixels[y] = PIX_MAKE(255, PIX_R(s->pixels[y]), PIX_G(s->pixels[y]), PIX_B(s->pixels[y]));
    return s;
}

Surface *
Bmp_Load(const wchar_t *path, wchar_t *error, size_t errorLen)
{
    size_t size = 0;
    uint8_t *data = Bmp_ReadFile(path, &size, error, errorLen);
    Surface *s;

    if (!data)
        return NULL;
    s = Bmp_Decode(data, size, error, errorLen);
    free(data);
    return s;
}

uint8_t *
Bmp_Encode(const Surface *s, size_t *size)
{
    BOOL opaque = TRUE;
    int bpp, x, y;
    size_t i, count = (size_t)s->width * s->height;
    size_t headerSize, stride, total, offset;
    uint8_t *data;
    BITMAPFILEHEADER *fh;

    for (i = 0; i < count; i++)
        if (PIX_A(s->pixels[i]) != 255)
        {
            opaque = FALSE;
            break;
        }

    bpp = opaque ? 24 : 32;
    // A V5 header has room for the colour masks and the alpha mask. The clipboard
    // (CF_DIBV5) wants exactly this layout, which is why we use it.
    headerSize = opaque ? sizeof(BITMAPINFOHEADER) : sizeof(BITMAPV5HEADER);
    stride = (((size_t)s->width * bpp + 31) / 32) * 4;
    offset = sizeof(BITMAPFILEHEADER) + headerSize;
    total = offset + stride * (size_t)s->height;

    data = (uint8_t *)calloc(1, total);
    if (!data)
        return NULL;

    fh = (BITMAPFILEHEADER *)data;
    fh->bfType = 0x4D42;
    fh->bfSize = (DWORD)total;
    fh->bfOffBits = (DWORD)offset;

    if (opaque)
    {
        BITMAPINFOHEADER *ih = (BITMAPINFOHEADER *)(data + sizeof(BITMAPFILEHEADER));
        ih->biSize = sizeof(BITMAPINFOHEADER);
        ih->biWidth = s->width;
        ih->biHeight = s->height;       // Positive means the file is stored bottom up
        ih->biPlanes = 1;
        ih->biBitCount = 24;
        ih->biCompression = BI_RGB;
        ih->biSizeImage = (DWORD)(stride * (size_t)s->height);
    }
    else
    {
        BITMAPV5HEADER *ih = (BITMAPV5HEADER *)(data + sizeof(BITMAPFILEHEADER));
        ih->bV5Size = sizeof(BITMAPV5HEADER);
        ih->bV5Width = s->width;
        ih->bV5Height = s->height;
        ih->bV5Planes = 1;
        ih->bV5BitCount = 32;
        ih->bV5Compression = BI_BITFIELDS;
        ih->bV5SizeImage = (DWORD)(stride * (size_t)s->height);
        ih->bV5RedMask = 0x00FF0000;
        ih->bV5GreenMask = 0x0000FF00;
        ih->bV5BlueMask = 0x000000FF;
        ih->bV5AlphaMask = 0xFF000000;
        ih->bV5CSType = 0x73524742;     // 'sRGB'
        ih->bV5Intent = LCS_GM_IMAGES;
    }

    for (y = 0; y < s->height; y++)
    {
        uint8_t *row = data + offset + stride * (size_t)(s->height - 1 - y);
        const uint32_t *in = s->pixels + (size_t)y * s->width;

        for (x = 0; x < s->width; x++)
        {
            if (opaque)
            {
                row[x * 3] = (uint8_t)PIX_B(in[x]);
                row[x * 3 + 1] = (uint8_t)PIX_G(in[x]);
                row[x * 3 + 2] = (uint8_t)PIX_R(in[x]);
            }
            else
            {
                uint32_t v = Pixel_Unpremultiply(in[x]); // Files hold normal colours
                memcpy(row + x * 4, &v, 4);
            }
        }
    }
    *size = total;
    return data;
}

BOOL
Bmp_Save(const Surface *s, const wchar_t *path, wchar_t *error, size_t errorLen)
{
    size_t size = 0;
    uint8_t *data = Bmp_Encode(s, &size);
    HANDLE file;
    DWORD written = 0;
    BOOL ok;

    if (!data)
    {
        Bmp_Fail(error, errorLen, L"There is not enough memory to save this image.");
        return FALSE;
    }
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        free(data);
        Bmp_Fail(error, errorLen, L"The file could not be created. Is it read only, or open in another program?");
        return FALSE;
    }
    ok = WriteFile(file, data, (DWORD)size, &written, NULL) && written == size;
    // CloseHandle can fail too, for example if a network drive goes away
    ok = CloseHandle(file) && ok;
    free(data);
    if (!ok)
        Bmp_Fail(error, errorLen, L"The file could not be written. Is the disk full?");
    return ok;
}
