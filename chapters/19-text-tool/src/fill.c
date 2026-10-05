/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: fill.c
 */

#include <stdlib.h>
#include "fill.h"
#include "pixel.h"

typedef struct Seed
{
    int x, y;
} Seed;

// Struct: SeedStack
// A list of places still to look at, that grows as needed.
typedef struct SeedStack
{
    Seed *items;
    size_t count;
    size_t capacity;
} SeedStack;

static BOOL
Stack_Push(SeedStack *st, int x, int y)
{
    if (st->count == st->capacity)
    {
        size_t newCap = st->capacity ? st->capacity * 2 : 256;
        Seed *bigger = (Seed *)realloc(st->items, newCap * sizeof(Seed));
        if (!bigger)
            return FALSE;
        st->items = bigger;
        st->capacity = newCap;
    }
    st->items[st->count].x = x;
    st->items[st->count].y = y;
    st->count++;
    return TRUE;
}

// Function: Fill_Close
// Is pixel b close enough to pixel a? Every channel must be within the tolerance.
static BOOL
Fill_Close(uint32_t a, uint32_t b, int tolerance)
{
    return abs((int)PIX_A(a) - (int)PIX_A(b)) <= tolerance
        && abs((int)PIX_R(a) - (int)PIX_R(b)) <= tolerance
        && abs((int)PIX_G(a) - (int)PIX_G(b)) <= tolerance
        && abs((int)PIX_B(a) - (int)PIX_B(b)) <= tolerance;
}

BOOL
Fill_FindRegion(const Surface *s, int x, int y, int tolerance, uint8_t *mask, RECT *bounds)
{
    SeedStack stack = { NULL, 0, 0 };
    uint32_t seedColor;
    int w = s->width, h = s->height;

    SetRectEmpty(bounds);
    if (x < 0 || y < 0 || x >= w || y >= h)
        return TRUE;

    seedColor = s->pixels[(size_t)y * w + x];
    bounds->left = x;
    bounds->right = x + 1;
    bounds->top = y;
    bounds->bottom = y + 1;

    // "Scanline" flood fill. Instead of one pixel at a time, we mark a whole
    // horizontal run, then look at the rows above and below for more runs.
    // The mask remembers which pixels we have already marked.
    if (!Stack_Push(&stack, x, y))
        return FALSE;
    while (stack.count > 0)
    {
        Seed seed = stack.items[--stack.count];
        int left = seed.x, right = seed.x;
        int dy, px;

        // Another run may have already marked this pixel
        if (mask[(size_t)seed.y * w + seed.x])
            continue;
        if (!Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + seed.x], tolerance))
            continue;

        // Stretch the run as far as we can both ways
        while (left > 0 && !mask[(size_t)seed.y * w + left - 1]
            && Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + left - 1], tolerance))
            left--;
        while (right < w - 1 && !mask[(size_t)seed.y * w + right + 1]
            && Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + right + 1], tolerance))
            right++;

        for (px = left; px <= right; px++)
            mask[(size_t)seed.y * w + px] = 255;
        bounds->left = min(bounds->left, left);
        bounds->right = max(bounds->right, right + 1);
        bounds->top = min(bounds->top, seed.y);
        bounds->bottom = max(bounds->bottom, seed.y + 1);

        // Look at the rows above and below. Each separate stretch of matching
        // pixels gets one seed.
        for (dy = -1; dy <= 1; dy += 2)
        {
            int ny = seed.y + dy;
            BOOL inRun = FALSE;

            if (ny < 0 || ny >= h)
                continue;
            for (px = left; px <= right; px++)
            {
                size_t i = (size_t)ny * w + px;
                BOOL ok = !mask[i] && Fill_Close(seedColor, s->pixels[i], tolerance);
                if (ok && !inRun && !Stack_Push(&stack, px, ny))
                {
                    free(stack.items);
                    return FALSE;
                }
                inRun = ok;
            }
        }
    }
    free(stack.items);
    return TRUE;
}

void
Fill_Flood(Edit *e, int x, int y, int tolerance)
{
    int w = e->base->width, h = e->base->height;
    uint8_t *found = (uint8_t *)calloc((size_t)w * h, 1);
    RECT area;
    int row;

    if (!found)
        return;
    // Look at the original pixels, not the ones we are painting
    if (Fill_FindRegion(e->base, x, y, tolerance, found, &area))
    {
        // Paint the region one run at a time
        for (row = area.top; row < area.bottom; row++)
        {
            int start = -1, col;
            for (col = area.left; col <= area.right; col++)
            {
                BOOL on = col < area.right && found[(size_t)row * w + col];
                if (on && start < 0)
                    start = col;
                else if (!on && start >= 0)
                {
                    Edit_FillSpan(e, row, start, col - 1);
                    start = -1;
                }
            }
        }
    }
    free(found);
}
