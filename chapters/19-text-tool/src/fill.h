/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: fill.h
 * The paint bucket.
 */
#ifndef FILL_H
#define FILL_H

#include "edit.h"

// Function: Fill_Flood
// Fills the area around (x, y) with the edit's fill colour. The area is every
// pixel you can reach from (x, y) by moving up, down, left or right without
// crossing a pixel that is too different from the one you started on.
//
// Parameters:
//   tolerance - How different a pixel may be and still count, 0 to 255.
//               0 means it must match exactly.
void Fill_Flood(Edit *e, int x, int y, int tolerance);

// Function: Fill_FindRegion
// Finds the pixels a flood fill from (x, y) would reach: the ones joined to the
// start by pixels that are close in colour to the start pixel. The magic wand
// uses this too.
//
// Parameters:
//   mask   - w * h bytes, all zero. Each reached pixel is set to 255.
//   bounds - Receives a rectangle around the reached pixels.
//
// Returns:
//   FALSE if we ran out of memory.
BOOL Fill_FindRegion(const Surface *s, int x, int y, int tolerance, uint8_t *mask, RECT *bounds);

#endif // FILL_H
