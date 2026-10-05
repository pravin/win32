/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: selection.h
 * A selection is a picture the same size as the image, with one byte per
 * pixel: 0 means "not selected", 255 means "selected". (Values in between mean
 * "partly selected", which we will not use for much, but which costs nothing.)
 *
 * Painting, filling and erasing only change selected pixels. When there is no
 * selection at all, mask is NULL and everything can be changed.
 */
#ifndef SELECTION_H
#define SELECTION_H

#include <windows.h>
#include <stdint.h>

// Struct: Selection
typedef struct Selection
{
    uint8_t *mask;      // One byte per pixel, or NULL if nothing is selected
    RECT bounds;        // A rectangle that surrounds every selected pixel
} Selection;

// Enum: SelMode
// What a new shape does to the selection that is already there.
typedef enum SelMode
{
    SEL_REPLACE,        // The new shape becomes the selection
    SEL_ADD,            // The selection grows to include the shape (Ctrl)
    SEL_SUBTRACT        // The shape is cut out of the selection (Alt)
} SelMode;

// Function: Sel_Clear
// Selects nothing.
void Sel_Clear(Selection *sel);

// Function: Sel_IsActive
// Is anything selected?
BOOL Sel_IsActive(const Selection *sel);

// Function: Sel_IsSelected
// Is the pixel (x, y) selected? If there is no selection, every pixel counts as selected.
BOOL Sel_IsSelected(const Selection *sel, int w, int x, int y);

// Function: Sel_SelectAll
// Selects the whole image. Returns FALSE if out of memory.
BOOL Sel_SelectAll(Selection *sel, int w, int h);

// Function: Sel_Invert
// Swaps selected and not selected. With no selection, selects everything.
BOOL Sel_Invert(Selection *sel, int w, int h);

// Function: Sel_CloneMask
// Makes a copy of the mask for the history. Returns NULL if there is no selection
// (or no memory).
uint8_t *Sel_CloneMask(const Selection *sel, int w, int h);

// Function: Sel_SetMask
// Replaces the selection with a copy of a mask. NULL means "select nothing".
BOOL Sel_SetMask(Selection *sel, int w, int h, const uint8_t *mask);

// Function: Sel_RecalcBounds
// Works out the bounds again after the mask has been edited. If nothing is
// selected any more, the mask is freed.
void Sel_RecalcBounds(Selection *sel, int w, int h);

// Function: Sel_ShapeRect
// Makes the coverage of a rectangle: 255 inside, 0 outside. The corners may be
// given in any order and may be outside the image. The result is clipped to
// the image, its size is returned in *bounds, and it has one byte per pixel
// of those bounds. Free it with free(). Returns NULL if empty or out of memory.
uint8_t *Sel_ShapeRect(int x0, int y0, int x1, int y1, int w, int h, RECT *bounds);

// Function: Sel_ShapeEllipse
// The same for the ellipse that fits in the rectangle.
uint8_t *Sel_ShapeEllipse(int x0, int y0, int x1, int y1, int w, int h, RECT *bounds);

// Function: Sel_ShapePolygon
// The same for the inside of a closed shape made of straight lines (the lasso).
uint8_t *Sel_ShapePolygon(const POINT *points, int count, int w, int h, RECT *bounds);

// Function: Sel_Combine
// Mixes a shape into the selection.
//
// Parameters:
//   sel         - The selection to change. Its mask must exist and be w * h bytes.
//   base        - What the selection was before the user started dragging (or NULL if nothing).
//   mode        - Replace, add or subtract.
//   shape       - The shape's coverage (from the Sel_Shape functions), or NULL for no shape yet.
//   shapeBounds - Where the shape is.
//   region      - In: everywhere we might have changed so far. Out: the same plus
//                 this shape. We recalculate all of it, so that a shape that
//                 shrinks as the mouse moves lets go of the pixels it leaves.
void Sel_Combine(Selection *sel, int w, int h, const uint8_t *base, SelMode mode,
    const uint8_t *shape, const RECT *shapeBounds, RECT *region);

#endif // SELECTION_H
