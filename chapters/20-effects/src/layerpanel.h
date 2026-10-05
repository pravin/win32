/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: layerpanel.h
 * The panel on the right: a list of layers, some buttons, an opacity slider
 * and a blend mode box.
 *
 * The panel looks at the document but never changes it. When the user does
 * something, it sends a message to its parent, and the main window decides
 * what to do. That keeps all the undo bookkeeping in one place.
 */
#ifndef LAYERPANEL_H
#define LAYERPANEL_H

#include <windows.h>
#include "doc.h"

// Message: WMU_LAYER_SELECT
// The user clicked a layer. wParam is its index in the stack.
#define WMU_LAYER_SELECT    (WM_APP + 5)

// Message: WMU_LAYER_VISIBLE
// The user clicked a layer's eye box. wParam is the layer index.
#define WMU_LAYER_VISIBLE   (WM_APP + 6)

// Message: WMU_LAYER_OPACITY
// The opacity slider moved. wParam is the new opacity, 0 to 255. lParam is
// TRUE if the user has let go of the slider (so it is time to remember it).
#define WMU_LAYER_OPACITY   (WM_APP + 7)

// Message: WMU_LAYER_BLEND
// A new blend mode was chosen. wParam is the BlendMode.
#define WMU_LAYER_BLEND     (WM_APP + 8)

// Message: WMU_LAYER_RENAME
// The user double clicked a layer to rename it. wParam is the layer index.
#define WMU_LAYER_RENAME    (WM_APP + 9)

// Function: LayerPanel_Create
// Creates the panel as a child of parent. Button clicks come back to the
// parent as WM_COMMAND, with the same IDM_LAYER_ ids as the Layer menu.
HWND LayerPanel_Create(HWND parent, HINSTANCE hInstance, int id);

// Function: LayerPanel_SetDocument
// Tells the panel which document to show. The panel does not own it.
void LayerPanel_SetDocument(HWND panel, Document *doc);

// Function: LayerPanel_Refresh
// Call this whenever the layers, or anything about them, has changed.
void LayerPanel_Refresh(HWND panel);

#endif // LAYERPANEL_H
