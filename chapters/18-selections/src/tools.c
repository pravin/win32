/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 *
 * File: tools.c
 */

#include <windows.h>
#include <stdlib.h>
#include "tools.h"
#include "fill.h"
#include "selection.h"
#include "pixel.h"
#include "composite.h"
#include "history.h"

static const wchar_t *g_toolNames[TOOL_COUNT] =
{
    L"Pencil", L"Brush", L"Eraser", L"Line",
    L"Rect", L"Ellipse", L"Fill", L"Picker",
    L"Rect Sel", L"Oval Sel", L"Lasso", L"Wand"
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

// Function: Tool_BeginEdit
// Starts an edit on the active layer, limited to the selection.
static BOOL
Tool_BeginEdit(ToolState *st, Document *doc)
{
    st->edit = Edit_Begin(Doc_ActiveSurface(doc));
    if (!st->edit)
        return FALSE;
    // With a selection, tools only change selected pixels. (NULL means everywhere.)
    Edit_SetClip(st->edit, doc->sel.mask);
    return TRUE;
}

// Function: Tool_Constrain
// Adjusts a drag (dx, dy) so that it is regular: a line at a multiple of 45
// degrees, or a square (and so a circle) for the other shapes.
static void
Tool_Constrain(ToolId tool, int *dx, int *dy)
{
    int adx = abs(*dx), ady = abs(*dy);
    int side = max(adx, ady);

    if (tool == TOOL_LINE)
    {
        if (adx > 2 * ady)
        {
            *dy = 0;
            return;
        }
        if (ady > 2 * adx)
        {
            *dx = 0;
            return;
        }
    }
    *dx = *dx < 0 ? -side : side;
    *dy = *dy < 0 ? -side : side;
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
    BOOL outline = ts->shapeStyle != SHAPE_FILLED;
    BOOL fill = ts->shapeStyle != SHAPE_OUTLINE;

    // Holding Shift keeps things regular: squares, circles, and lines at 45 degree steps
    if (keys & MK_SHIFT)
        Tool_Constrain(ts->tool, &dx, &dy);

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

// Function: Tool_SelectShape
// Builds the shape the mouse is dragging out, for the current select tool.
// Returns the coverage array (free() it), or NULL if there is nothing yet.
static uint8_t *
Tool_SelectShape(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys, RECT *bounds)
{
    int dx = x - st->startX, dy = y - st->startY;

    switch (ts->tool)
    {
    case TOOL_SELECT_RECT:
    case TOOL_SELECT_ELLIPSE:
        if (keys & MK_SHIFT)
            Tool_Constrain(ts->tool, &dx, &dy);
        if (ts->tool == TOOL_SELECT_RECT)
            return Sel_ShapeRect(st->startX, st->startY, st->startX + dx, st->startY + dy, doc->width, doc->height, bounds);
        return Sel_ShapeEllipse(st->startX, st->startY, st->startX + dx, st->startY + dy, doc->width, doc->height, bounds);
    case TOOL_LASSO:
        return Sel_ShapePolygon(st->lasso, st->lassoCount, doc->width, doc->height, bounds);
    default:
        return NULL;
    }
}

// Function: Tool_SelectApply
// Recalculates the selection from the shape and the selection we started from.
static void
Tool_SelectApply(ToolState *st, Document *doc, const uint8_t *shape, const RECT *shapeBounds)
{
    Sel_Combine(&doc->sel, doc->width, doc->height, st->selBase, st->selMode, shape, shapeBounds, &st->selRegion);

    // While dragging we do not know the exact bounds, but the region we have
    // touched is a good enough rectangle for drawing the outline.
    {
        RECT image = { 0, 0, doc->width, doc->height };
        IntersectRect(&doc->sel.bounds, &st->selRegion, &image);
    }
    st->overlay = st->selRegion;
}

// Function: Tool_SelectBegin
// The mouse went down with a selection tool.
static void
Tool_SelectBegin(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys)
{
    size_t size = (size_t)doc->width * doc->height;
    uint8_t *mask;

    // Ctrl adds to the selection, Alt takes away from it
    st->selMode = (keys & MK_CONTROL) ? SEL_ADD : ((GetKeyState(VK_MENU) & 0x8000) ? SEL_SUBTRACT : SEL_REPLACE);
    st->selBase = Sel_CloneMask(&doc->sel, doc->width, doc->height);

    // Start from the old selection, unless we are replacing it
    mask = (uint8_t *)calloc(size, 1);
    if (!mask)
    {
        free(st->selBase);
        st->selBase = NULL;
        return;
    }
    if (st->selMode != SEL_REPLACE && st->selBase)
        memcpy(mask, st->selBase, size);
    st->selRegion = doc->sel.bounds;        // Everything that was selected needs a repaint
    Sel_Clear(&doc->sel);
    doc->sel.mask = mask;
    st->selecting = TRUE;
    st->overlay = st->selRegion;

    if (ts->tool == TOOL_LASSO)
    {
        st->lassoCount = 0;
        if (!st->lasso)
        {
            st->lassoCapacity = 256;
            st->lasso = (POINT *)malloc((size_t)st->lassoCapacity * sizeof(POINT));
        }
        if (st->lasso)
        {
            st->lasso[0].x = x;
            st->lasso[0].y = y;
            st->lassoCount = 1;
        }
    }
    else if (ts->tool == TOOL_WAND)
    {
        // The wand selects everything the flood fill would have filled
        uint8_t *found = (uint8_t *)calloc(size, 1);
        RECT whole = { 0, 0, doc->width, doc->height };
        RECT b;

        if (found)
        {
            Fill_FindRegion(Doc_ActiveSurface(doc), x, y, ts->tolerance, found, &b);
            Tool_SelectApply(st, doc, found, &whole);
            free(found);
        }
    }
    else
    {
        // A rectangle or ellipse starts as a single pixel
        RECT b;
        uint8_t *shape = Tool_SelectShape(st, doc, ts, x, y, keys, &b);
        Tool_SelectApply(st, doc, shape, &b);
        free(shape);
    }
}

// Function: Tool_SelectMove
// The mouse moved with a selection tool, button down.
static void
Tool_SelectMove(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys)
{
    RECT b;
    uint8_t *shape;

    if (ts->tool == TOOL_WAND)
        return;
    if (ts->tool == TOOL_LASSO)
    {
        if (!st->lasso || (x == st->lasso[st->lassoCount - 1].x && y == st->lasso[st->lassoCount - 1].y))
            return;
        if (st->lassoCount == st->lassoCapacity)
        {
            POINT *bigger = (POINT *)realloc(st->lasso, (size_t)st->lassoCapacity * 2 * sizeof(POINT));
            if (!bigger)
                return;
            st->lasso = bigger;
            st->lassoCapacity *= 2;
        }
        st->lasso[st->lassoCount].x = x;
        st->lasso[st->lassoCount].y = y;
        st->lassoCount++;
    }
    shape = Tool_SelectShape(st, doc, ts, x, y, keys, &b);
    Tool_SelectApply(st, doc, shape, shape ? &b : NULL);
    free(shape);
}

// Function: Tool_SelectEnd
// The mouse button came up. The selection is final, so remember it for undo.
static void
Tool_SelectEnd(ToolState *st, Document *doc)
{
    uint8_t *after;
    BOOL changed;

    // Count the real bounds now. If nothing is selected, this removes the mask.
    Sel_RecalcBounds(&doc->sel, doc->width, doc->height);
    after = Sel_CloneMask(&doc->sel, doc->width, doc->height);

    changed = (st->selBase != NULL) != (after != NULL)
        || (st->selBase && after && memcmp(st->selBase, after, (size_t)doc->width * doc->height) != 0);
    if (changed)
    {
        // The history keeps both masks now
        History_PushSelection(doc->history, st->selBase, after, doc->width, doc->height);
    }
    else
    {
        free(st->selBase);
        free(after);
    }
    st->selBase = NULL;
    st->selecting = FALSE;
    st->overlay = st->selRegion;
}

// Function: Tool_IsSelectTool
static BOOL
Tool_IsSelectTool(ToolId tool)
{
    return tool == TOOL_SELECT_RECT || tool == TOOL_SELECT_ELLIPSE || tool == TOOL_LASSO || tool == TOOL_WAND;
}

void
Tool_MouseDown(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, BOOL useSecond, UINT keys)
{
    uint32_t color = useSecond ? ts->secondary : ts->primary;

    st->startX = x;
    st->startY = y;
    st->useSecond = useSecond;

    if (Tool_IsSelectTool(ts->tool))
    {
        Tool_SelectBegin(st, doc, ts, x, y, keys);
        st->lastX = x;
        st->lastY = y;
        return;
    }

    switch (ts->tool)
    {
    case TOOL_PENCIL:
    case TOOL_BRUSH:
        if (!Tool_BeginEdit(st, doc))
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
        if (!Tool_BeginEdit(st, doc))
            return;
        Edit_SetErase(st->edit, TRUE);
        Edit_SetBrush(st->edit, (ts->primary & 0xFF000000) | 0x00FFFFFF, ts->brushSize, ts->softBrush);
        Edit_Stamp(st->edit, x, y);
        break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
        if (!Tool_BeginEdit(st, doc))
            return;
        Tool_DrawShape(st, ts, x, y, keys);
        break;
    case TOOL_FILL:
        if (!Tool_BeginEdit(st, doc))
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
    if (st->selecting)
    {
        Tool_SelectMove(st, doc, ts, x, y, keys);
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
    if (st->selecting)
    {
        Tool_SelectEnd(st, doc);
        return;
    }
    if (!st->edit)
        return;

    doc->modified = TRUE;
    // The pixels are already where they should be. We keep a before and an
    // after copy of the area that changed, so the user can undo it.
    History_PushPixels(doc->history, doc->active, st->edit->base, Doc_ActiveSurface(doc), &st->edit->bounds);
    Edit_End(st->edit);
    st->edit = NULL;
}

void
Tool_Release(ToolState *st)
{
    free(st->lasso);
    free(st->selBase);
    st->lasso = NULL;
    st->selBase = NULL;
}

BOOL
Tool_TakeOverlay(ToolState *st, RECT *area)
{
    if (IsRectEmpty(&st->overlay))
        return FALSE;
    *area = st->overlay;
    SetRectEmpty(&st->overlay);
    return TRUE;
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
