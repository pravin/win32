/* DrawLite - Win32 Tutorial
 * Chapter 8 - Pencil and brush
 *
 * File: edit.c
 */

#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "edit.h"
#include "pixel.h"

Edit *
Edit_Begin(Surface *target)
{
    Edit *e = (Edit *)calloc(1, sizeof(Edit));
    if (!e)
        return NULL;

    e->target = target;
    e->base = Surface_Clone(target);
    e->mask = (uint8_t *)calloc((size_t)target->width * target->height, 1);
    if (!e->base || !e->mask)
    {
        Edit_End(e);
        return NULL;
    }
    e->color = 0xFF000000;
    e->size = 1;
    return e;
}

void
Edit_End(Edit *e)
{
    if (!e)
        return;
    Surface_Destroy(e->base);
    free(e->mask);
    free(e);
}

void
Edit_SetBrush(Edit *e, uint32_t color, int size, BOOL soft)
{
    e->color = color;
    e->size = size < 1 ? 1 : size;
    e->soft = soft;
}

BOOL
Edit_TakeDirty(Edit *e, RECT *area)
{
    if (IsRectEmpty(&e->dirty))
        return FALSE;
    *area = e->dirty;
    SetRectEmpty(&e->dirty);
    return TRUE;
}

// Function: Edit_Touch
// Remembers that we have changed this area.
static void
Edit_Touch(Edit *e, const RECT *area)
{
    RECT merged;
    UnionRect(&merged, &e->bounds, area);
    e->bounds = merged;
    UnionRect(&merged, &e->dirty, area);
    e->dirty = merged;
}

// Function: Edit_Coverage
// How much of the pixel at distance dist from the brush centre does the brush cover?
// Returns 0.0 to 1.0.
static float
Edit_Coverage(const Edit *e, float dist, float radius)
{
    if (e->soft)
    {
        // Fade out smoothly from the middle to the edge
        float t = dist / radius;
        if (t >= 1.0f)
            return 0.0f;
        return 1.0f - t * t * (3.0f - 2.0f * t);
    }
    else
    {
        // Solid, with a one pixel soft edge so circles do not look jagged
        float c = radius + 0.5f - dist;
        return c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
    }
}

void
Edit_Stamp(Edit *e, int cx, int cy)
{
    Surface *s = e->target;
    float radius = e->size / 2.0f;
    int reach = (int)ceilf(radius);
    RECT area;
    int x, y;
    uint32_t colorA = PIX_A(e->color);

    area.left = max(cx - reach, 0);
    area.top = max(cy - reach, 0);
    area.right = min(cx + reach + 1, s->width);
    area.bottom = min(cy + reach + 1, s->height);
    if (IsRectEmpty(&area))
        return;

    for (y = area.top; y < area.bottom; y++)
    {
        for (x = area.left; x < area.right; x++)
        {
            size_t i = (size_t)y * s->width + x;
            float dist = sqrtf((float)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
            uint32_t m = (uint32_t)(Edit_Coverage(e, dist, radius) * 255.0f + 0.5f);
            uint32_t a, src;

            // Keep the strongest coverage any stamp has given this pixel.
            // This is why a see-through stroke does not get darker where it overlaps itself.
            if (m <= e->mask[i])
                continue;
            e->mask[i] = (uint8_t)m;

            // Work out the paint at this strength and lay it over the original pixel
            a = Pixel_Mul255(colorA, m);
            src = Pixel_Premultiply((a << 24) | (e->color & 0x00FFFFFF));
            s->pixels[i] = Pixel_Over(src, e->base->pixels[i]);
        }
    }
    Edit_Touch(e, &area);
}

void
Edit_Line(Edit *e, int x0, int y0, int x1, int y1)
{
    // Bresenham's line algorithm. It walks from one end to the other using only
    // whole numbers, deciding at each step whether to move sideways, up/down, or both.
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        int e2;

        Edit_Stamp(e, x0, y0);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}
