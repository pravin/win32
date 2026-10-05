/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: effects.h
 * Filters: invert, grayscale, blur and friends.
 *
 * Some of these take a long time on a big picture, and a program that does
 * not answer its messages for ten seconds is a program Windows will mark as
 * "Not Responding". So the work happens on a second thread, while the main
 * thread shows a progress box with a Cancel button. The effect never touches
 * the real layer. It reads a copy and writes a copy, and only when it has
 * finished successfully do we move the result across and tell the history.
 */
#ifndef EFFECTS_H
#define EFFECTS_H

#include <windows.h>
#include "doc.h"

typedef enum EffectId
{
    FX_INVERT,
    FX_GRAYSCALE,
    FX_SEPIA,
    FX_BRIGHTEN,        // param: how much, -255 to 255
    FX_BLUR,            // param: radius in pixels
    FX_PIXELATE,        // param: block size in pixels
    FX_COUNT
} EffectId;

// Function: Effect_Run
// Applies an effect to the active layer, inside the selection if there is one.
// Shows a progress box if it takes more than a moment. The change is one step in the history.
//
// Parameters:
//   area - Receives the part of the image that changed (when TRUE is returned).
//
// Returns:
//   TRUE if the layer changed. FALSE if the user cancelled or something went wrong.
BOOL Effect_Run(HINSTANCE hInstance, HWND hParent, Document *doc, EffectId id, int param, RECT *area);

#endif // EFFECTS_H
