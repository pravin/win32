/* DrawLite - Win32 Tutorial
 * Chapter 7 - Pixels you own
 *
 * File: tools.c
 */

#include <wchar.h>
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
