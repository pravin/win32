/* DrawLite - Win32 Tutorial
 * Chapter 9 - Colour
 *
 * File: canvas.h
 * The window you draw in.
 */
#ifndef CANVAS_H
#define CANVAS_H

#include <windows.h>
#include "doc.h"
#include "tools.h"

// Message: WMU_CANVAS_POS
// Posted to the canvas's parent when the mouse moves over the canvas.
// wParam is the x position in image pixels, lParam is y. They can be negative
// or past the edge of the image.
#define WMU_CANVAS_POS  (WM_APP + 1)

// Function: Canvas_Create
// Creates the canvas as a child of parent. Its size is set later by the
// parent's layout code.
HWND Canvas_Create(HWND parent, HINSTANCE hInstance, int id);

// Function: Canvas_SetDocument
// Tells the canvas which document to show. The canvas does not own it.
// Pass NULL to show nothing.
void Canvas_SetDocument(HWND canvas, Document *doc);

// Function: Canvas_SetSettings
// Tells the canvas where to find the current tool, colours and brush size.
// The canvas does not own them. It looks at them every time it draws.
void Canvas_SetSettings(HWND canvas, const ToolSettings *settings);

#endif // CANVAS_H
