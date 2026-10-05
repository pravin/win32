/* DrawLite - Win32 Tutorial
 * Chapter 6 - The canvas
 *
 * File: canvas.h
 * The window you draw in.
 */
#ifndef CANVAS_H
#define CANVAS_H

#include <windows.h>

// Function: Canvas_Create
// Creates the canvas as a child of parent. Its size is set later by the
// parent's layout code.
//
// Returns:
//   The canvas window, or NULL on failure.
HWND Canvas_Create(HWND parent, HINSTANCE hInstance, int id);

#endif // CANVAS_H
