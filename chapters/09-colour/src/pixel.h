/* DrawLite - Win32 Tutorial
 * Chapter 9 - Colour
 *
 * File: pixel.h
 * What a pixel is, and a little arithmetic on them.
 *
 * A pixel is a uint32_t laid out as 0xAARRGGBB. Windows calls this "BGRA"
 * because on a little-endian PC the bytes sit in memory as B, G, R, A.
 * We keep every pixel PREMULTIPLIED: red, green and blue have already been
 * multiplied by alpha. A half see-through red is 0x80800000, not 0x80FF0000.
 * It looks odd for a minute, then it makes the blending maths much shorter.
 */
#ifndef PIXEL_H
#define PIXEL_H

#include <stdint.h>

#define PIX_A(c)    ((uint32_t)((c) >> 24) & 0xFF)
#define PIX_R(c)    ((uint32_t)((c) >> 16) & 0xFF)
#define PIX_G(c)    ((uint32_t)((c) >> 8) & 0xFF)
#define PIX_B(c)    ((uint32_t)(c) & 0xFF)

#define PIX_MAKE(a, r, g, b) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#define PIX_WHITE   0xFFFFFFFFu
#define PIX_BLACK   0xFF000000u

// Function: Pixel_Mul255
// Returns a * b / 255, rounded. Both are 0 to 255.
static inline uint32_t
Pixel_Mul255(uint32_t a, uint32_t b)
{
    uint32_t t = a * b + 128;
    return (t + (t >> 8)) >> 8; // A fast, exact way of dividing by 255
}

// Function: Pixel_Premultiply
// Turns a normal ("straight") 0xAARRGGBB colour into a premultiplied pixel.
static inline uint32_t
Pixel_Premultiply(uint32_t c)
{
    uint32_t a = PIX_A(c);
    if (a == 255)
        return c;
    return PIX_MAKE(a, Pixel_Mul255(PIX_R(c), a), Pixel_Mul255(PIX_G(c), a), Pixel_Mul255(PIX_B(c), a));
}

// Function: Pixel_Unpremultiply
// The opposite of Pixel_Premultiply. Fully transparent pixels come back as 0.
static inline uint32_t
Pixel_Unpremultiply(uint32_t p)
{
    uint32_t a = PIX_A(p);
    uint32_t r, g, b;

    if (a == 255 || a == 0)
        return a == 0 ? 0 : p;
    r = (PIX_R(p) * 255 + a / 2) / a;
    g = (PIX_G(p) * 255 + a / 2) / a;
    b = (PIX_B(p) * 255 + a / 2) / a;
    return PIX_MAKE(a, r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
}

// Function: Pixel_FromColorRef
// Windows describes colours as a COLORREF, 0x00BBGGRR (red is the LOW byte).
// This turns one into our 0xAARRGGBB form, with the alpha you give it.
static inline uint32_t
Pixel_FromColorRef(uint32_t colorRef, uint32_t alpha)
{
    return PIX_MAKE(alpha, colorRef & 0xFF, (colorRef >> 8) & 0xFF, (colorRef >> 16) & 0xFF);
}

// Function: Pixel_ToColorRef
// The other way round. The alpha is dropped.
static inline uint32_t
Pixel_ToColorRef(uint32_t c)
{
    return PIX_R(c) | (PIX_G(c) << 8) | (PIX_B(c) << 16);
}

// Function: Pixel_Over
// Puts the premultiplied pixel src on top of the premultiplied pixel dst.
// This is the classic "over" operator:  result = src + dst * (1 - src alpha)
static inline uint32_t
Pixel_Over(uint32_t src, uint32_t dst)
{
    uint32_t inv = 255 - PIX_A(src);
    if (inv == 0)
        return src;
    if (inv == 255)
        return dst;
    return PIX_MAKE(
        PIX_A(src) + Pixel_Mul255(PIX_A(dst), inv),
        PIX_R(src) + Pixel_Mul255(PIX_R(dst), inv),
        PIX_G(src) + Pixel_Mul255(PIX_G(dst), inv),
        PIX_B(src) + Pixel_Mul255(PIX_B(dst), inv));
}

#endif // PIXEL_H
