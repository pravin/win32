/* DrawLite - Win32 Tutorial
 * Chapter 14 - Open and save
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

void
Fill_Flood(Edit *e, int x, int y, int tolerance)
{
    Surface *s = e->base;       // Look at the original pixels, not the ones we are painting
    SeedStack stack = { NULL, 0, 0 };
    uint32_t seedColor;
    int w = s->width, h = s->height;

    if (x < 0 || y < 0 || x >= w || y >= h)
        return;

    // We use the fill mask to remember which pixels we have already filled.
    // Filling an empty span makes sure the mask exists.
    Edit_FillSpan(e, y, x, x);
    seedColor = s->pixels[(size_t)y * w + x];

    // "Scanline" flood fill. Instead of one pixel at a time, we fill a whole
    // horizontal run, then look at the rows above and below for more runs.
    Stack_Push(&stack, x, y);
    while (stack.count > 0)
    {
        Seed seed = stack.items[--stack.count];
        int left = seed.x, right = seed.x;
        int dy;

        // Another run may have already filled this pixel
        if (e->fillMask[(size_t)seed.y * w + seed.x] && !(seed.x == x && seed.y == y))
            continue;
        if (!Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + seed.x], tolerance))
            continue;

        // Stretch the run as far as we can both ways
        while (left > 0 && !e->fillMask[(size_t)seed.y * w + left - 1]
            && Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + left - 1], tolerance))
            left--;
        while (right < w - 1 && !e->fillMask[(size_t)seed.y * w + right + 1]
            && Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + right + 1], tolerance))
            right++;

        Edit_FillSpan(e, seed.y, left, right);

        // Look at the rows above and below. Each separate stretch of matching
        // pixels gets one seed.
        for (dy = -1; dy <= 1; dy += 2)
        {
            int ny = seed.y + dy;
            int px;
            BOOL inRun = FALSE;

            if (ny < 0 || ny >= h)
                continue;
            for (px = left; px <= right; px++)
            {
                size_t i = (size_t)ny * w + px;
                BOOL ok = !e->fillMask[i] && Fill_Close(seedColor, s->pixels[i], tolerance);
                if (ok && !inRun)
                    Stack_Push(&stack, px, ny);
                inRun = ok;
            }
        }
    }
    free(stack.items);
}
