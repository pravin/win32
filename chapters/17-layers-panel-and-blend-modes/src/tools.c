/* DrawLite - Win32 Tutorial
 * Chapter 17 - Layers panel and blend modes
 *
 * File: tools.c
 */

#include <windows.h>
#include <stdlib.h>
#include "tools.h"
#include "fill.h"
#include "pixel.h"
#include "composite.h"
#include "history.h"

static const wchar_t *g_toolNames[TOOL_COUNT] =
{
    L"Pencil", L"Brush", L"Eraser", L"Line",
    L"Rect", L"Ellipse", L"Fill", L"Picker"
};

const wchar_t *
Tool_Name(ToolId tool)
{
    if (tool < 0 || tool >= TOOL_COUNT)
        return L"";
    return g_toolNames[tool];
}

// Function: Tool_Collect
// Moves the area the edit has changed into the tool state, so it is still
// there after the edit has been freed.
static void
Tool_Collect(ToolState *st)
{
    RECT area, merged;

    if (st->edit && Edit_TakeDirty(st->edit, &area))
    {
        UnionRect(&merged, &st->dirty, &area);
        st->dirty = merged;
    }
}

// Function: Tool_DrawShape
// Draws the line, rectangle or ellipse from where the mouse went down to (x, y).
// It first undoes the previous attempt, so as you drag you see the shape grow and shrink.
static void
Tool_DrawShape(ToolState *st, ToolSettings *ts, int x, int y, UINT keys)
{
    uint32_t mainColor = st->useSecond ? ts->secondary : ts->primary;
    uint32_t other = st->useSecond ? ts->primary : ts->secondary;
    int x0 = st->startX, y0 = st->startY;
    int dx = x - x0, dy = y - y0;
    int adx = abs(dx), ady = abs(dy);
    BOOL outline = ts->shapeStyle != SHAPE_FILLED;
    BOOL fill = ts->shapeStyle != SHAPE_OUTLINE;

    // Holding Shift keeps things regular: squares, circles, and lines at 45 degree steps
    if (keys & MK_SHIFT)
    {
        if (ts->tool == TOOL_LINE)
        {
            if (adx > 2 * ady)
                dy = 0;
            else if (ady > 2 * adx)
                dx = 0;
            else
            {
                int side = max(adx, ady);
                dx = dx < 0 ? -side : side;
                dy = dy < 0 ? -side : side;
            }
        }
        else
        {
            int side = max(adx, ady);
            dx = dx < 0 ? -side : side;
            dy = dy < 0 ? -side : side;
        }
    }

    Edit_Reset(st->edit);
    Edit_SetBrush(st->edit, mainColor, ts->brushSize, ts->softBrush);
    // A filled shape with no outline uses the main colour for its inside
    Edit_SetFillColor(st->edit, ts->shapeStyle == SHAPE_FILLED ? mainColor : other);

    switch (ts->tool)
    {
    case TOOL_LINE:
        Edit_Line(st->edit, x0, y0, x0 + dx, y0 + dy);
        break;
    case TOOL_RECT:
        Edit_Rect(st->edit, x0, y0, x0 + dx, y0 + dy, outline, fill);
        break;
    case TOOL_ELLIPSE:
        Edit_Ellipse(st->edit, x0, y0, x0 + dx, y0 + dy, outline, fill);
        break;
    default:
        break;
    }
}

// Function: Tool_Pick
// The eyedropper. Copies the colour under the mouse into the primary
// (or secondary) colour.
static void
Tool_Pick(ToolState *st, Document *doc, ToolSettings *ts, int x, int y)
{
    uint32_t pixel;

    if (x < 0 || y < 0 || x >= doc->width || y >= doc->height)
        return;

    // Pixels are premultiplied, colours in the settings are not
    // Pick what you can see: all the visible layers added together
    pixel = Pixel_Unpremultiply(Doc_PixelAt(doc, x, y));
    if (st->useSecond)
        ts->secondary = pixel;
    else
        ts->primary = pixel;
    st->colorsChanged = TRUE;
}

void
Tool_MouseDown(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, BOOL useSecond, UINT keys)
{
    uint32_t color = useSecond ? ts->secondary : ts->primary;

    st->startX = x;
    st->startY = y;
    st->useSecond = useSecond;

    switch (ts->tool)
    {
    case TOOL_PENCIL:
    case TOOL_BRUSH:
        st->edit = Edit_Begin(Doc_ActiveSurface(doc));
        if (!st->edit)
            return;
        // The pencil is always one hard pixel wide. The brush uses the settings.
        if (ts->tool == TOOL_PENCIL)
            Edit_SetBrush(st->edit, color, 1, FALSE);
        else
            Edit_SetBrush(st->edit, color, ts->brushSize, ts->softBrush);
        Edit_Stamp(st->edit, x, y); // A single click leaves a dot
        break;
    case TOOL_ERASER:
        // With layers, the eraser makes pixels see-through. The alpha slider
        // sets how hard it rubs.
        st->edit = Edit_Begin(Doc_ActiveSurface(doc));
        if (!st->edit)
            return;
        Edit_SetErase(st->edit, TRUE);
        Edit_SetBrush(st->edit, (ts->primary & 0xFF000000) | 0x00FFFFFF, ts->brushSize, ts->softBrush);
        Edit_Stamp(st->edit, x, y);
        break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
        st->edit = Edit_Begin(Doc_ActiveSurface(doc));
        if (!st->edit)
            return;
        Tool_DrawShape(st, ts, x, y, keys);
        break;
    case TOOL_FILL:
        st->edit = Edit_Begin(Doc_ActiveSurface(doc));
        if (!st->edit)
            return;
        // The fill colour is the colour of the button you pressed
        Edit_SetFillColor(st->edit, color);
        Fill_Flood(st->edit, x, y, ts->tolerance);
        break;
    case TOOL_PICKER:
        Tool_Pick(st, doc, ts, x, y);
        break;
    default:
        break;
    }
    st->lastX = x;
    st->lastY = y;
    Tool_Collect(st);
}

void
Tool_MouseMove(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys)
{
    if (ts->tool == TOOL_PICKER)
    {
        // Dragging the picker keeps picking
        Tool_Pick(st, doc, ts, x, y);
        return;
    }
    if (!st->edit)
        return;

    switch (ts->tool)
    {
    case TOOL_PENCIL:
    case TOOL_BRUSH:
    case TOOL_ERASER:
        // Mouse messages only arrive every so often, so a fast stroke is a
        // series of dots with gaps. We join them with straight lines.
        Edit_Line(st->edit, st->lastX, st->lastY, x, y);
        break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
        Tool_DrawShape(st, ts, x, y, keys);
        break;
    default:
        break;
    }
    st->lastX = x;
    st->lastY = y;
    Tool_Collect(st);
}

void
Tool_MouseUp(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys)
{
    Tool_MouseMove(st, doc, ts, x, y, keys);
    if (!st->edit)
        return;

    doc->modified = TRUE;
    // The pixels are already where they should be. We keep a before and an
    // after copy of the area that changed, so the user can undo it.
    History_PushPixels(doc->history, doc->active, st->edit->base, Doc_ActiveSurface(doc), &st->edit->bounds);
    Edit_End(st->edit);
    st->edit = NULL;
}

BOOL
Tool_TakeDirty(ToolState *st, RECT *area)
{
    if (IsRectEmpty(&st->dirty))
        return FALSE;
    *area = st->dirty;
    SetRectEmpty(&st->dirty);
    return TRUE;
}
