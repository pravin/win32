/* DrawLite - Win32 Tutorial
 * Chapter 17 - Layers panel and blend modes
 *
 * File: blend.c
 *
 * The recipe, from the W3C "Compositing and Blending" specification.
 * Call the layer on top the source (s) and what is underneath the backdrop (b).
 * Cs and Cb are their colours, as, ab their alphas, all between 0 and 1.
 * B(Cb, Cs) is the blend function that makes each mode different. Then:
 *
 *   result alpha = as + ab - as * ab
 *   result colour (times result alpha) =
 *         as * (1 - ab) * Cs          the part of the source with nothing under it
 *       + as * ab * B(Cb, Cs)         the part where the two overlap
 *       + (1 - as) * ab * Cb          the part of the backdrop with nothing over it
 *
 * Normal mode is B(Cb, Cs) = Cs, which simplifies to the "over" we already have.
 */

#include <math.h>
#include <wchar.h>
#include "blend.h"
#include "pixel.h"

static const wchar_t *g_names[BLEND_COUNT] =
{
    L"Normal", L"Multiply", L"Screen", L"Overlay", L"Darken", L"Lighten",
    L"Color Dodge", L"Color Burn", L"Hard Light", L"Soft Light", L"Difference", L"Exclusion"
};

const wchar_t *
Blend_Name(BlendMode mode)
{
    return (mode >= 0 && mode < BLEND_COUNT) ? g_names[mode] : L"";
}

static float
Blend_HardLight(float cb, float cs)
{
    if (cs <= 0.5f)
        return cb * 2.0f * cs;              // Multiply
    return cb + (2.0f * cs - 1.0f) - cb * (2.0f * cs - 1.0f);   // Screen
}

static float
Blend_SoftLight(float cb, float cs)
{
    if (cs <= 0.5f)
        return cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
    else
    {
        float d = (cb <= 0.25f) ? ((16.0f * cb - 12.0f) * cb + 4.0f) * cb : sqrtf(cb);
        return cb + (2.0f * cs - 1.0f) * (d - cb);
    }
}

// Function: Blend_Channel
// B(Cb, Cs) for one colour channel.
static float
Blend_Channel(BlendMode mode, float cb, float cs)
{
    switch (mode)
    {
    case BLEND_MULTIPLY:    return cb * cs;
    case BLEND_SCREEN:      return cb + cs - cb * cs;
    case BLEND_OVERLAY:     return Blend_HardLight(cs, cb);     // Hard light with the layers swapped
    case BLEND_DARKEN:      return cb < cs ? cb : cs;
    case BLEND_LIGHTEN:     return cb > cs ? cb : cs;
    case BLEND_COLOR_DODGE:
        if (cb == 0.0f)
            return 0.0f;
        if (cs >= 1.0f)
            return 1.0f;
        return fminf(1.0f, cb / (1.0f - cs));
    case BLEND_COLOR_BURN:
        if (cb >= 1.0f)
            return 1.0f;
        if (cs <= 0.0f)
            return 0.0f;
        return 1.0f - fminf(1.0f, (1.0f - cb) / cs);
    case BLEND_HARD_LIGHT:  return Blend_HardLight(cb, cs);
    case BLEND_SOFT_LIGHT:  return Blend_SoftLight(cb, cs);
    case BLEND_DIFFERENCE:  return fabsf(cb - cs);
    case BLEND_EXCLUSION:   return cb + cs - 2.0f * cb * cs;
    default:                return cs;
    }
}

// Function: Blend_ToByte
// Turns 0.0 to 1.0 into 0 to 255, rounding and keeping inside the range.
static uint32_t
Blend_ToByte(float v)
{
    int i = (int)(v * 255.0f + 0.5f);
    return (uint32_t)(i < 0 ? 0 : (i > 255 ? 255 : i));
}

uint32_t
Blend_Pixel(BlendMode mode, uint32_t src, uint32_t dst)
{
    uint32_t sa = PIX_A(src), da = PIX_A(dst);
    float as, ab, ao;
    float cs[3], cb[3], out[3];
    int i;

    // These cases do not need any maths, and are most pixels in most pictures
    if (mode == BLEND_NORMAL || sa == 0 || da == 0)
        return Pixel_Over(src, dst);

    // Our pixels are premultiplied, but the formula wants plain colours
    as = sa / 255.0f;
    ab = da / 255.0f;
    cs[0] = PIX_R(src) / 255.0f / as;
    cs[1] = PIX_G(src) / 255.0f / as;
    cs[2] = PIX_B(src) / 255.0f / as;
    cb[0] = PIX_R(dst) / 255.0f / ab;
    cb[1] = PIX_G(dst) / 255.0f / ab;
    cb[2] = PIX_B(dst) / 255.0f / ab;

    ao = as + ab - as * ab;
    for (i = 0; i < 3; i++)
    {
        // The result is premultiplied too, which is what we want to store
        out[i] = as * (1.0f - ab) * cs[i]
               + as * ab * Blend_Channel(mode, cb[i], cs[i])
               + (1.0f - as) * ab * cb[i];
    }
    return PIX_MAKE(Blend_ToByte(ao), Blend_ToByte(out[0]), Blend_ToByte(out[1]), Blend_ToByte(out[2]));
}
