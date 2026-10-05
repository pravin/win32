/* DrawLite - Win32 Tutorial
 * Chapter 5 - Toolbar and status bar
 *
 * File: tools.h
 * The list of drawing tools.
 */
#ifndef TOOLS_H
#define TOOLS_H

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

// Function: Tool_Name
// Returns the display name of a tool, for buttons and the status bar.
const wchar_t *Tool_Name(ToolId tool);

#endif // TOOLS_H
