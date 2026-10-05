/* DrawLite - Win32 Tutorial
 * Chapter 13 - Undo and redo
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
    e->fillColor = 0xFFFFFFFF;
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
    free(e->fillMask);
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

// Function: Edit_Paint
// Returns the premultiplied pixel you get from painting a (normal) colour at
// the given strength, 0 to 255.
static uint32_t
Edit_Paint(uint32_t color, uint32_t strength)
{
    uint32_t a = Pixel_Mul255(PIX_A(color), strength);
    return Pixel_Premultiply((a << 24) | (color & 0x00FFFFFF));
}

// Function: Edit_Compose
// Works out the final pixels for an area: the original, then the fill on top,
// then the outline on top of that. Nothing is added to what is already
// there, so doing it twice gives the same answer as doing it once.
static void
Edit_Compose(Edit *e, const RECT *area)
{
    Surface *s = e->target;
    int x, y;

    for (y = area->top; y < area->bottom; y++)
    {
        for (x = area->left; x < area->right; x++)
        {
            size_t i = (size_t)y * s->width + x;
            uint32_t p = e->base->pixels[i];

            if (e->fillMask && e->fillMask[i])
                p = Pixel_Over(Edit_Paint(e->fillColor, e->fillMask[i]), p);
            if (e->mask[i])
                p = Pixel_Over(Edit_Paint(e->color, e->mask[i]), p);
            s->pixels[i] = p;
        }
    }
}

// Function: Edit_Clip
// Trims a rectangle so it lies inside the surface.
static void
Edit_Clip(const Edit *e, RECT *area)
{
    area->left = max(area->left, 0);
    area->top = max(area->top, 0);
    area->right = min(area->right, e->target->width);
    area->bottom = min(area->bottom, e->target->height);
}

void
Edit_Stamp(Edit *e, int cx, int cy)
{
    Surface *s = e->target;
    float radius = e->size / 2.0f;
    int reach = (int)ceilf(radius);
    RECT area;
    int x, y;

    SetRect(&area, cx - reach, cy - reach, cx + reach + 1, cy + reach + 1);
    Edit_Clip(e, &area);
    if (IsRectEmpty(&area))
        return;

    for (y = area.top; y < area.bottom; y++)
    {
        for (x = area.left; x < area.right; x++)
        {
            size_t i = (size_t)y * s->width + x;
            float dist = sqrtf((float)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
            uint32_t m = (uint32_t)(Edit_Coverage(e, dist, radius) * 255.0f + 0.5f);

            // Keep the strongest coverage any stamp has given this pixel.
            // This is why a see-through stroke does not get darker where it overlaps itself.
            if (m > e->mask[i])
                e->mask[i] = (uint8_t)m;
        }
    }
    Edit_Compose(e, &area);
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

void
Edit_SetFillColor(Edit *e, uint32_t color)
{
    e->fillColor = color;
}

void
Edit_Reset(Edit *e)
{
    RECT area = e->bounds;
    int y;

    if (IsRectEmpty(&area))
        return;

    // Forget the paint in the area we have touched, then put the original pixels back
    for (y = area.top; y < area.bottom; y++)
    {
        size_t row = (size_t)y * e->target->width + area.left;
        size_t count = (size_t)(area.right - area.left);

        memset(e->mask + row, 0, count);
        if (e->fillMask)
            memset(e->fillMask + row, 0, count);
        memcpy(e->target->pixels + row, e->base->pixels + row, count * sizeof(uint32_t));
    }
    Edit_Touch(e, &area);
}

void
Edit_FillSpan(Edit *e, int y, int x0, int x1)
{
    RECT area;
    int x;

    if (y < 0 || y >= e->target->height)
        return;
    x0 = max(x0, 0);
    x1 = min(x1, e->target->width - 1);
    if (x0 > x1)
        return;

    if (!e->fillMask)
    {
        e->fillMask = (uint8_t *)calloc((size_t)e->target->width * e->target->height, 1);
        if (!e->fillMask)
            return;
    }
    for (x = x0; x <= x1; x++)
        e->fillMask[(size_t)y * e->target->width + x] = 255;

    SetRect(&area, x0, y, x1 + 1, y + 1);
    Edit_Compose(e, &area);
    Edit_Touch(e, &area);
}

// Function: Edit_Order
// Makes sure x0 <= x1 and y0 <= y1.
static void
Edit_Order(int *x0, int *y0, int *x1, int *y1)
{
    int t;
    if (*x0 > *x1) { t = *x0; *x0 = *x1; *x1 = t; }
    if (*y0 > *y1) { t = *y0; *y0 = *y1; *y1 = t; }
}

void
Edit_Rect(Edit *e, int x0, int y0, int x1, int y1, BOOL outline, BOOL fill)
{
    int y;
    // Stroke along the inside edge so the outer edge of the line sits on the box we dragged
    int inset = (e->size - 1) / 2;

    Edit_Order(&x0, &y0, &x1, &y1);

    if (fill)
        for (y = y0; y <= y1; y++)
            Edit_FillSpan(e, y, x0, x1);

    if (outline)
    {
        int l = min(x0 + inset, (x0 + x1) / 2), r = max(x1 - inset, (x0 + x1 + 1) / 2);
        int t = min(y0 + inset, (y0 + y1) / 2), b = max(y1 - inset, (y0 + y1 + 1) / 2);

        Edit_Line(e, l, t, r, t);
        Edit_Line(e, r, t, r, b);
        Edit_Line(e, r, b, l, b);
        Edit_Line(e, l, b, l, t);
    }
}

void
Edit_Ellipse(Edit *e, int x0, int y0, int x1, int y1, BOOL outline, BOOL fill)
{
    // Work in pixel edges: the box covers x0 to x1+1, so its centre is half way between.
    double cx, cy, rx, ry;
    double inset = (e->size - 1) / 2.0;
    int y;

    Edit_Order(&x0, &y0, &x1, &y1);
    cx = (x0 + x1 + 1) / 2.0;
    cy = (y0 + y1 + 1) / 2.0;
    rx = (x1 - x0 + 1) / 2.0;
    ry = (y1 - y0 + 1) / 2.0;

    if (fill)
    {
        for (y = y0; y <= y1; y++)
        {
            // How far from the middle row are we, as a fraction of the height?
            double dy = (y + 0.5 - cy) / ry;
            double half;
            if (dy > 1.0 || dy < -1.0)
                continue;
            // From x^2/rx^2 + y^2/ry^2 = 1, the half width of this row
            half = rx * sqrt(1.0 - dy * dy);
            Edit_FillSpan(e, y, (int)ceil(cx - half - 0.5), (int)floor(cx + half - 0.5));
        }
    }

    if (outline)
    {
        // The path runs through the middle of the outermost pixels, so it is half a pixel
        // smaller than the box, and a thick brush needs room on the inside as well.
        double orx = max(rx - 0.5 - inset, 0.0), ory = max(ry - 0.5 - inset, 0.0);
        int steps = max(16, (int)(4.0 * (orx + ory)));
        int i, px = 0, py = 0;

        // Walk around the ellipse in small steps and join the dots with lines
        for (i = 0; i <= steps; i++)
        {
            double angle = 6.283185307179586 * i / steps;
            int x = (int)floor(cx - 0.5 + orx * cos(angle) + 0.5);
            int yy = (int)floor(cy - 0.5 + ory * sin(angle) + 0.5);
            if (i == 0)
                Edit_Stamp(e, x, yy);
            else
                Edit_Line(e, px, py, x, yy);
            px = x;
            py = yy;
        }
    }
}
