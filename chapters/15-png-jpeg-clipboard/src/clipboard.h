/* DrawLite - Win32 Tutorial
 * Chapter 15 - PNG, JPEG and the clipboard
 *
 * File: clipboard.h
 * Copying pictures to and from the Windows clipboard.
 */
#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <windows.h>
#include "surface.h"

// Function: Clipboard_CopyImage
// Puts a picture on the clipboard in two formats: "PNG", which keeps the
// transparency and is understood by most modern programs, and a device
// independent bitmap (DIB), which every Windows program understands.
//
// Returns:
//   TRUE if it worked.
BOOL Clipboard_CopyImage(HWND owner, const Surface *s);

// Function: Clipboard_HasImage
// Is there a picture on the clipboard we can paste?
BOOL Clipboard_HasImage(void);

// Function: Clipboard_PasteImage
// Makes a new surface from the picture on the clipboard.
// Returns NULL (and a message in error) if there is not one.
Surface *Clipboard_PasteImage(HWND owner, wchar_t *error, size_t errorLen);

#endif // CLIPBOARD_H
