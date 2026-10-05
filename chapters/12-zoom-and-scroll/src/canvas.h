/* DrawLite - Win32 Tutorial
 * Chapter 12 - Zoom and scroll
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

// Message: WMU_COLORS_CHANGED
// Posted to the canvas's parent when a tool (the picker) has changed the
// primary or secondary colour, so the palette can redraw its swatches.
#define WMU_COLORS_CHANGED  (WM_APP + 2)

// Message: WMU_ZOOM_CHANGED
// Posted to the canvas's parent when the zoom level changes.
// wParam is the new zoom in percent.
#define WMU_ZOOM_CHANGED    (WM_APP + 3)

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
void Canvas_SetSettings(HWND canvas, ToolSettings *settings);

// Function: Canvas_SetZoom
// Sets the zoom in percent (100 is actual size). The point (cx, cy), in canvas
// window co-ordinates, stays where it is on screen. Pass -1, -1 to use the
// middle of the window. The value is clamped to the range we allow.
void Canvas_SetZoom(HWND canvas, int percent, int cx, int cy);

// Function: Canvas_ZoomStep
// Moves to the next zoom level up (direction +1) or down (-1).
void Canvas_ZoomStep(HWND canvas, int direction, int cx, int cy);

// Function: Canvas_ZoomToFit
// Picks the biggest zoom at which the whole image is visible.
void Canvas_ZoomToFit(HWND canvas);

// Function: Canvas_GetZoom
// Returns the zoom in percent.
int Canvas_GetZoom(HWND canvas);

#endif // CANVAS_H
