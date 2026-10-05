/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: blend.h
 * Ways of combining a layer with the layers below it.
 */
#ifndef BLEND_H
#define BLEND_H

#include <stdint.h>

// Enum: BlendMode
// Normal just puts the layer on top. The others mix the colours with what is
// underneath. The names and the maths are the standard ones, the same ones
// used by Photoshop, Paint.NET, CSS (mix-blend-mode) and most others.
typedef enum BlendMode
{
    BLEND_NORMAL,
    BLEND_MULTIPLY,
    BLEND_SCREEN,
    BLEND_OVERLAY,
    BLEND_DARKEN,
    BLEND_LIGHTEN,
    BLEND_COLOR_DODGE,
    BLEND_COLOR_BURN,
    BLEND_HARD_LIGHT,
    BLEND_SOFT_LIGHT,
    BLEND_DIFFERENCE,
    BLEND_EXCLUSION,
    BLEND_COUNT
} BlendMode;

// Function: Blend_Name
// The name to show the user.
const wchar_t *Blend_Name(BlendMode mode);

// Function: Blend_Pixel
// Puts the premultiplied pixel src on top of the premultiplied pixel dst,
// mixing them the way the mode says. Returns a premultiplied pixel.
uint32_t Blend_Pixel(BlendMode mode, uint32_t src, uint32_t dst);

#endif // BLEND_H
