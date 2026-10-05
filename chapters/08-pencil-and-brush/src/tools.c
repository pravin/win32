/* DrawLite - Win32 Tutorial
 * Chapter 8 - Pencil and brush
 *
 * File: tools.c
 */

#include <windows.h>
#include "tools.h"

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

void
Tool_MouseDown(ToolState *st, Document *doc, const ToolSettings *ts, int x, int y, BOOL useSecond)
{
    uint32_t color = useSecond ? ts->secondary : ts->primary;

    switch (ts->tool)
    {
    case TOOL_PENCIL:
    case TOOL_BRUSH:
        st->edit = Edit_Begin(doc->surface);
        if (!st->edit)
            return;
        // The pencil is always one hard pixel wide. The brush uses the settings.
        if (ts->tool == TOOL_PENCIL)
            Edit_SetBrush(st->edit, color, 1, FALSE);
        else
            Edit_SetBrush(st->edit, color, ts->brushSize, ts->softBrush);
        Edit_Stamp(st->edit, x, y); // A single click leaves a dot
        break;
    default:
        break; // The other tools come in later chapters
    }
    st->lastX = x;
    st->lastY = y;
    Tool_Collect(st);
}

void
Tool_MouseMove(ToolState *st, Document *doc, const ToolSettings *ts, int x, int y)
{
    UNREFERENCED_PARAMETER(doc);
    UNREFERENCED_PARAMETER(ts);

    if (!st->edit)
        return;

    // Mouse messages only arrive every so often, so a fast stroke is a
    // series of dots with gaps. We join them with straight lines.
    Edit_Line(st->edit, st->lastX, st->lastY, x, y);
    st->lastX = x;
    st->lastY = y;
    Tool_Collect(st);
}

void
Tool_MouseUp(ToolState *st, Document *doc, const ToolSettings *ts, int x, int y)
{
    Tool_MouseMove(st, doc, ts, x, y);
    if (!st->edit)
        return;

    doc->modified = TRUE;
    Tool_Collect(st);
    // The pixels are already where they should be, so there is nothing
    // left to do but let go of the edit.
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
