/* DrawLite - Win32 Tutorial
 * Chapter 17 - Layers panel and blend modes
 *
 * File: surface.h
 * A rectangle of pixels that we can read and write directly.
 */
#ifndef SURFACE_H
#define SURFACE_H

#include <windows.h>
#include <stdint.h>

// Struct: Surface
// A 32 bit per pixel image. The pixels are stored row by row, top row first,
// with no gaps between the rows. Pixel (x, y) is pixels[y * width + x].
typedef struct Surface
{
    int width;
    int height;
    uint32_t *pixels;   // Premultiplied 0xAARRGGBB, see pixel.h
    HBITMAP bitmap;     // The same memory as a GDI bitmap. Windows frees it, not us.
} Surface;

// Function: Surface_Create
// Makes a new surface where every pixel is 0 (fully transparent black).
// Returns NULL if the size is silly or Windows is out of memory.
Surface *Surface_Create(int width, int height);

// Function: Surface_Destroy
// Frees a surface. Safe to call with NULL.
void Surface_Destroy(Surface *s);

// Function: Surface_Clone
// Makes an exact copy of a surface.
Surface *Surface_Clone(const Surface *s);

// Function: Surface_Fill
// Sets every pixel to the given premultiplied pixel.
void Surface_Fill(Surface *s, uint32_t pixel);

// Function: Surface_GetPixel
// Returns the pixel at (x, y), or 0 if that is outside the surface.
uint32_t Surface_GetPixel(const Surface *s, int x, int y);

// Function: Surface_SetPixel
// Replaces the pixel at (x, y). Does nothing if it is outside the surface.
void Surface_SetPixel(Surface *s, int x, int y, uint32_t pixel);

#endif // SURFACE_H
