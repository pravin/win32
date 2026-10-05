/* DrawLite - Win32 Tutorial
 * Chapter 8 - Pencil and brush
 *
 * File: tools.h
 * The drawing tools, and the settings they share.
 */
#ifndef TOOLS_H
#define TOOLS_H

#include <windows.h>
#include <stdint.h>
#include "doc.h"
#include "edit.h"

// Enum: ToolId
// Every tool DrawLite has. The order here is the order in the palette.
typedef enum ToolId
{
    TOOL_PENCIL,
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_FILL,
    TOOL_PICKER,
    TOOL_COUNT
} ToolId;

// Struct: ToolSettings
// What the user has chosen. The main window owns one of these and the
// canvas looks at it whenever it needs to draw.
typedef struct ToolSettings
{
    ToolId tool;
    uint32_t primary;       // Left button colour, normal 0xAARRGGBB
    uint32_t secondary;     // Right button colour
    int brushSize;          // Width in pixels
    BOOL softBrush;
} ToolSettings;

// Struct: ToolState
// What a tool remembers while the mouse button is held down.
typedef struct ToolState
{
    Edit *edit;             // The change in progress, or NULL when no button is down
    int lastX, lastY;       // Where the mouse was last time
    RECT dirty;             // Changed area the canvas has not repainted yet
} ToolState;

// Function: Tool_Name
// Returns the display name of a tool, for buttons and the status bar.
const wchar_t *Tool_Name(ToolId tool);

// Function: Tool_MouseDown
// Called when a mouse button goes down on the canvas.
//
// Parameters:
//   x, y      - Position in image pixels
//   useSecond - TRUE if it was the right button, so we paint with the secondary colour
void Tool_MouseDown(ToolState *st, Document *doc, const ToolSettings *ts, int x, int y, BOOL useSecond);

// Function: Tool_MouseMove
// Called when the mouse moves while the button is down.
void Tool_MouseMove(ToolState *st, Document *doc, const ToolSettings *ts, int x, int y);

// Function: Tool_MouseUp
// Called when the button comes back up. Finishes the edit.
void Tool_MouseUp(ToolState *st, Document *doc, const ToolSettings *ts, int x, int y);

// Function: Tool_TakeDirty
// Returns the area of the image that changed since the last call.
BOOL Tool_TakeDirty(ToolState *st, RECT *area);

#endif // TOOLS_H
