/* DrawLite - Win32 Tutorial
 * Chapter 7 - Pixels you own
 *
 * File: palette.h
 * The tool palette: a column of buttons down the left of the window.
 */
#ifndef PALETTE_H
#define PALETTE_H

#include <windows.h>
#include "tools.h"

// Function: Palette_Create
// Creates the palette as a child of parent. It forwards button clicks to the
// parent as WM_COMMAND messages with the IDM_TOOL_ ids.
HWND Palette_Create(HWND parent, HINSTANCE hInstance, int id);

// Function: Palette_SetTool
// Shows which tool is selected.
void Palette_SetTool(HWND palette, ToolId tool);

#endif // PALETTE_H
