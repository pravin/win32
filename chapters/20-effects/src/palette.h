/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: palette.h
 * The tool palette: tool buttons, the two paint colours, a few quick
 * colours and an alpha slider.
 */
#ifndef PALETTE_H
#define PALETTE_H

#include <windows.h>
#include "tools.h"

// Function: Palette_Create
// Creates the palette as a child of parent. It forwards tool button clicks to
// the parent as WM_COMMAND messages with the IDM_TOOL_ ids. Colour changes it
// writes straight into settings.
HWND Palette_Create(HWND parent, HINSTANCE hInstance, int id, ToolSettings *settings);

// Function: Palette_SetTool
// Shows which tool is selected.
void Palette_SetTool(HWND palette, ToolId tool);

// Function: Palette_Refresh
// Redraws the colour swatches. Call it after changing settings->primary or
// settings->secondary from outside the palette.
void Palette_Refresh(HWND palette);

// Function: Palette_SwapColors
// Swaps the primary and secondary colours.
void Palette_SwapColors(HWND palette);

#endif // PALETTE_H
