/* DrawLite - Win32 Tutorial
 * Chapter 16 - Layers
 *
 * File: composite.h
 * Squashing the layers together into one picture.
 */
#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <windows.h>
#include "doc.h"

// Function: Doc_UpdateView
// Redraws doc->view inside "area" (image co-ordinates): a chequerboard, then
// every visible layer on top of it, bottom first. The canvas shows doc->view.
// Call it whenever a layer changes. Only the changed area is recalculated.
void Doc_UpdateView(Document *doc, const RECT *area);

// Function: Doc_UpdateAllOfView
// The same for the whole image.
void Doc_UpdateAllOfView(Document *doc);

// Function: Doc_Flatten
// Makes a new surface with all the visible layers squashed together. There is
// no chequerboard: see-through parts stay see-through. This is what we save
// and copy. Free it with Surface_Destroy. Returns NULL if out of memory.
Surface *Doc_Flatten(const Document *doc);

// Function: Doc_PixelAt
// The colour of the picture at (x, y), all visible layers combined. Premultiplied.
uint32_t Doc_PixelAt(const Document *doc, int x, int y);

#endif // COMPOSITE_H
