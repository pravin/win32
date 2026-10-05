/* DrawLite - Win32 Tutorial
 * Chapter 12 - Zoom and scroll
 *
 * File: surface.c
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "surface.h"

#define SURFACE_MAX_SIZE 16384

Surface *
Surface_Create(int width, int height)
{
    BITMAPINFO bi;
    Surface *s;
    void *bits = NULL;
    HBITMAP bitmap;

    if (width < 1 || height < 1 || width > SURFACE_MAX_SIZE || height > SURFACE_MAX_SIZE)
        return NULL;

    // Describe the bitmap we want: 32 bits per pixel, no compression.
    // A NEGATIVE height means the top row comes first in memory,
    // which is how we like to think about images.
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    // Windows allocates the memory and hands us a pointer to it in "bits"
    bitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bitmap)
        return NULL;

    s = (Surface *)calloc(1, sizeof(Surface));
    if (!s)
    {
        DeleteObject(bitmap);
        return NULL;
    }
    s->width = width;
    s->height = height;
    s->pixels = (uint32_t *)bits;
    s->bitmap = bitmap;
    return s;
}

void
Surface_Destroy(Surface *s)
{
    if (!s)
        return;
    DeleteObject(s->bitmap); // This frees the pixel memory too
    free(s);
}

Surface *
Surface_Clone(const Surface *s)
{
    Surface *copy = Surface_Create(s->width, s->height);
    if (copy)
        memcpy(copy->pixels, s->pixels, (size_t)s->width * s->height * sizeof(uint32_t));
    return copy;
}

void
Surface_Fill(Surface *s, uint32_t pixel)
{
    size_t i, count = (size_t)s->width * s->height;
    for (i = 0; i < count; i++)
        s->pixels[i] = pixel;
}

uint32_t
Surface_GetPixel(const Surface *s, int x, int y)
{
    if (x < 0 || y < 0 || x >= s->width || y >= s->height)
        return 0;
    return s->pixels[(size_t)y * s->width + x];
}

void
Surface_SetPixel(Surface *s, int x, int y, uint32_t pixel)
{
    if (x < 0 || y < 0 || x >= s->width || y >= s->height)
        return;
    s->pixels[(size_t)y * s->width + x] = pixel;
}
