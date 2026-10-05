/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: dlt.h
 * DrawLite's own file format (.dlt), the only one that remembers layers.
 *
 * The layout, all numbers little endian 32 bit:
 *
 *   "DLT1"                     magic, so we can tell what it is
 *   width, height              of the image
 *   layer count, active layer
 *   then for each layer, bottom first:
 *       name                   32 UTF-16 characters (64 bytes), zero padded
 *       visible                0 or 1
 *       opacity                0 to 255
 *       blend mode             a BlendMode number
 *       pixels                 width * height premultiplied BGRA pixels, row by row
 *
 * No compression. It is easy to read and write, which is the point. If your
 * files get big, a run length encoding of the pixels would be a good first
 * improvement.
 */
#ifndef DLT_H
#define DLT_H

#include <windows.h>
#include "doc.h"

// Function: Dlt_IsDltPath
// Does this file name end in .dlt?
BOOL Dlt_IsDltPath(const wchar_t *path);

// Function: Dlt_Save
// Writes a whole document, layers and all.
BOOL Dlt_Save(const Document *doc, const wchar_t *path, wchar_t *error, size_t errorLen);

// Function: Dlt_Load
// Reads a document. Returns NULL with a message in error on failure.
Document *Dlt_Load(const wchar_t *path, wchar_t *error, size_t errorLen);

#endif // DLT_H
