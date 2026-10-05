/* DrawLite - Win32 Tutorial
 * Chapter 13 - Undo and redo
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

#endif // FILL_H
