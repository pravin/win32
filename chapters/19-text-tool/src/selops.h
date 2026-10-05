/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: selops.h
 * Things you can do with a selection: copy it, cut it, delete it.
 */
#ifndef SELOPS_H
#define SELOPS_H

#include <windows.h>
#include "doc.h"

// Function: SelOps_CopyPixels
// Makes a new surface holding the selected part of the active layer, cut to
// the size of the selection. Pixels outside the shape are see-through.
// Returns NULL if nothing is selected, or out of memory. Free it with Surface_Destroy.
Surface *SelOps_CopyPixels(const Document *doc);

// Function: SelOps_ClearPixels
// Makes the selected part of the active layer see-through, and records it for
// undo. Returns FALSE if nothing is selected.
BOOL SelOps_ClearPixels(Document *doc);

#endif // SELOPS_H
