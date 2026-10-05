/* DrawLite - Win32 Tutorial
 * Chapter 9 - Colour
 *
 * File: edit.h
 * One change to a surface, from the moment the mouse goes down until it
 * comes back up. A brush stroke is an Edit. So, soon, is a rectangle.
 *
 * An Edit keeps a copy of the surface as it was before we started (the "base").
 * That copy lets us paint see-through colours properly, and later it will let
 * us undo.
 */
#ifndef EDIT_H
#define EDIT_H

#include <windows.h>
#include <stdint.h>
#include "surface.h"

// Struct: Edit
typedef struct Edit
{
    Surface *target;    // The surface we are painting on
    Surface *base;      // What it looked like before we started
    uint8_t *mask;      // How much paint each pixel has had, 0 (none) to 255 (full)
    uint32_t color;     // Paint colour, normal (not premultiplied) 0xAARRGGBB
    int size;           // Brush width in pixels
    BOOL soft;          // Soft edged brush?
    RECT bounds;        // Everything we have touched so far
    RECT dirty;         // Touched since the canvas last asked
} Edit;

// Function: Edit_Begin
// Starts an edit on a surface. Returns NULL if we run out of memory.
Edit *Edit_Begin(Surface *target);

// Function: Edit_End
// Frees an edit. The changes stay on the surface.
void Edit_End(Edit *e);

// Function: Edit_SetBrush
// Chooses the paint colour, brush width and softness.
void Edit_SetBrush(Edit *e, uint32_t color, int size, BOOL soft);

// Function: Edit_Stamp
// Presses the brush on the surface once, centred on pixel (cx, cy).
void Edit_Stamp(Edit *e, int cx, int cy);

// Function: Edit_Line
// Drags the brush in a straight line from (x0, y0) to (x1, y1).
void Edit_Line(Edit *e, int x0, int y0, int x1, int y1);

// Function: Edit_TakeDirty
// Gives you the area that changed since the last call, so you can repaint it.
// Returns FALSE if nothing changed.
BOOL Edit_TakeDirty(Edit *e, RECT *area);

#endif // EDIT_H
