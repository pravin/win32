/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: bmp.h
 * Reads and writes Windows bitmap (.bmp) files.
 *
 * We could ask Windows to do this (LoadImage, GetDIBits). We do it by hand,
 * because a bitmap file is about the simplest image format there is, and
 * because knowing what is inside one makes the rest of the API make sense.
 */
#ifndef BMP_H
#define BMP_H

#include <windows.h>
#include "surface.h"

// Function: Bmp_Load
// Reads a .bmp file. Understands 1, 4, 8, 24 and 32 bits per pixel, with or
// without a palette, top down or bottom up. It does not understand compressed
// (RLE or JPEG inside BMP) files.
//
// Parameters:
//   path     - The file to read
//   error    - Receives a message if it fails. Can be NULL.
//   errorLen - Size of error, in characters
//
// Returns:
//   A new surface, or NULL on failure. Free it with Surface_Destroy.
Surface *Bmp_Load(const wchar_t *path, wchar_t *error, size_t errorLen);

// Function: Bmp_Save
// Writes a surface as a .bmp file. If every pixel is opaque we write the
// usual 24 bit file. If not, we write 32 bits and keep the transparency.
//
// Returns:
//   TRUE on success. On failure FALSE and a message in error.
BOOL Bmp_Save(const Surface *s, const wchar_t *path, wchar_t *error, size_t errorLen);

// Function: Bmp_Decode
// The same as Bmp_Load, but from memory. Lets the clipboard code (and our tests)
// reuse the reader.
Surface *Bmp_Decode(const uint8_t *data, size_t size, wchar_t *error, size_t errorLen);

// Function: Bmp_Encode
// The same as Bmp_Save, but into a block of memory that you free() afterwards.
uint8_t *Bmp_Encode(const Surface *s, size_t *size);

#endif // BMP_H
