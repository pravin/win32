/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
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
#include "selection.h"

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
    TOOL_SELECT_RECT,
    TOOL_SELECT_ELLIPSE,
    TOOL_LASSO,
    TOOL_WAND,
    TOOL_TEXT,
    TOOL_COUNT
} ToolId;

// Enum: ShapeStyle
// How the rectangle and ellipse tools draw.
typedef enum ShapeStyle
{
    SHAPE_OUTLINE,      // Just the outline, in the paint colour
    SHAPE_FILLED,       // Just the inside, in the paint colour
    SHAPE_BOTH          // Outline in the paint colour, inside in the other colour
} ShapeStyle;

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
    ShapeStyle shapeStyle;
    int tolerance;          // How different colours may be for the fill and the wand, 0 to 255
    LOGFONTW font;          // The font for the text tool. lfHeight is in image pixels.
} ToolSettings;

// Struct: ToolState
// What a tool remembers while the mouse button is held down.
typedef struct ToolState
{
    Edit *edit;             // The change in progress, or NULL when no button is down
    int lastX, lastY;       // Where the mouse was last time
    int startX, startY;     // Where the mouse went down
    BOOL useSecond;         // Was it the right button?
    BOOL colorsChanged;     // The picker changed a colour. The canvas tells the palette.
    RECT dirty;             // Changed pixels the canvas has not recombined and repainted yet

    // The selection tools
    BOOL selecting;         // Are we in the middle of making a selection?
    SelMode selMode;        // Replace, add or subtract
    uint8_t *selBase;       // The selection as it was when the mouse went down (or NULL)
    RECT selRegion;         // Everywhere the selection may have changed during this drag
    POINT *lasso;           // The points of the lasso so far
    int lassoCount;
    int lassoCapacity;
    RECT overlay;           // Part of the picture whose selection outline needs repainting
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
//   keys      - The MK_ flags from the mouse message. We use MK_SHIFT.
void Tool_MouseDown(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, BOOL useSecond, UINT keys);

// Function: Tool_MouseMove
// Called when the mouse moves while the button is down.
void Tool_MouseMove(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys);

// Function: Tool_MouseUp
// Called when the button comes back up. Finishes the edit.
void Tool_MouseUp(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys);

// Function: Tool_PaintText
// Paints a block of text, given as a coverage mask, onto the active layer with
// its top left at (x, y). It is one undoable step. Returns TRUE if anything changed.
BOOL Tool_PaintText(ToolState *st, Document *doc, ToolSettings *ts, BOOL useSecond,
    int x, int y, int w, int h, const uint8_t *coverage);

// Function: Tool_Release
// Frees what a ToolState has allocated. Call it when the canvas goes away.
void Tool_Release(ToolState *st);

// Function: Tool_TakeOverlay
// Returns the area where only the selection outline changed (the pixels did not).
BOOL Tool_TakeOverlay(ToolState *st, RECT *area);

// Function: Tool_TakeDirty
// Returns the area of the image that changed since the last call.
BOOL Tool_TakeDirty(ToolState *st, RECT *area);

#endif // TOOLS_H
