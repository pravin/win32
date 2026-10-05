/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 *
 * File: imageio.h
 * Loads and saves pictures in whatever format the file name says.
 *
 * BMP we read and write ourselves (bmp.c). For everything else we ask Windows:
 * it ships with the Windows Imaging Component, "WIC", which knows PNG, JPEG,
 * GIF, TIFF and more. WIC is a COM API, which looks scary the first time but
 * is really just a set of structs full of function pointers. In C we call them
 * through macros that COBJMACROS gives us.
 */
#ifndef IMAGEIO_H
#define IMAGEIO_H

#include <windows.h>
#include <stdint.h>
#include "surface.h"

// Function: ImageIO_Init
// Sets up COM and WIC. Call once, first thing in wWinMain.
// Returns FALSE if Windows could not give us WIC (it is part of every
// Windows since Vista, so this should not happen).
BOOL ImageIO_Init(void);

// Function: ImageIO_Shutdown
// Undoes ImageIO_Init.
void ImageIO_Shutdown(void);

// Function: ImageIO_Load
// Loads a picture. The format is worked out from the file's contents, not its name.
// Returns a new surface, or NULL with a message in error.
Surface *ImageIO_Load(const wchar_t *path, wchar_t *error, size_t errorLen);

// Function: ImageIO_LoadMemory
// The same, from a block of memory (for example PNG data from the clipboard).
Surface *ImageIO_LoadMemory(const uint8_t *data, size_t size, wchar_t *error, size_t errorLen);

// Function: ImageIO_Save
// Saves a picture. The format is chosen from the extension: .png, .jpg, .jpeg
// or .bmp. Anything else is an error.
BOOL ImageIO_Save(const Surface *s, const wchar_t *path, wchar_t *error, size_t errorLen);

// Function: ImageIO_CanSave
// Can we save a file with this name? (Is the extension one we know?)
BOOL ImageIO_CanSave(const wchar_t *path);

// Function: ImageIO_EncodePng
// Turns a surface into PNG data in a block of memory. Free it with free().
uint8_t *ImageIO_EncodePng(const Surface *s, size_t *size);

#endif // IMAGEIO_H
