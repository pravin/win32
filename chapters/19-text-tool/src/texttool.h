/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: texttool.h
 * Typing text onto the picture.
 *
 * While you type, the text lives in a real EDIT control that floats over the
 * canvas. The edit control gives us a caret, selection, copy and paste,
 * Backspace and everything else for free. When you are done we draw the text
 * ourselves into a coverage mask, and paint through that, so the text becomes
 * ordinary pixels on the active layer and can be undone like anything else.
 */
#ifndef TEXTTOOL_H
#define TEXTTOOL_H

#include <windows.h>
#include <stdint.h>

// Message: WMU_TEXT_DONE
// Posted to the canvas by the edit control when the user wants to finish.
// wParam is the serial number of the text box (so we can ignore messages from
// a box that is already gone). lParam is TRUE to keep the text, FALSE to throw it away.
#define WMU_TEXT_DONE   (WM_APP + 10)

// Struct: TextBox
// The floating edit control and everything it needs.
typedef struct TextBox
{
    HWND hwnd;              // The EDIT control
    HWND canvas;            // Its parent
    HFONT font;             // The font it shows, scaled for the zoom level
    HBRUSH background;      // Colour behind the text while editing
    COLORREF textColor;     // Colour of the text while editing
    COLORREF backColor;     // Colour behind it
    LOGFONTW logFont;       // The font we will draw with, sized in IMAGE pixels
    uint32_t color;         // The paint colour, normal 0xAARRGGBB
    int imageX, imageY;     // Top left of the text, in image pixels
    int zoom;               // Zoom (percent) when the box was made
    UINT dpi;
    UINT serial;            // Which text box this is. Goes up by one each time.
} TextBox;

// Function: Text_Create
// Makes the floating edit control with its top left at window position
// (winX, winY) on the canvas, which is image pixel (imageX, imageY).
// logFont's height is in image pixels (negative, like LOGFONT usually is).
// Returns NULL on failure. Free it with Text_Destroy.
TextBox *Text_Create(HWND canvas, const LOGFONTW *logFont, uint32_t color, int zoom,
    int winX, int winY, int imageX, int imageY);

// Function: Text_Destroy
// Destroys the edit control and frees everything.
void Text_Destroy(TextBox *tb);

// Function: Text_GetString
// The text typed so far, in a new buffer you free() yourself. Never NULL.
wchar_t *Text_GetString(const TextBox *tb);

// Function: Text_Rasterize
// Draws text with a font into a new coverage mask: one byte per pixel, 255
// where the text is fully inked and 0 where there is nothing.
//
// Parameters:
//   logFont - The font. lfHeight is in pixels.
//   w, h    - Receive the size of the mask.
//
// Returns:
//   The mask (free() it), or NULL if there is no text or no memory.
uint8_t *Text_Rasterize(const LOGFONTW *logFont, const wchar_t *text, int *w, int *h);

#endif // TEXTTOOL_H
