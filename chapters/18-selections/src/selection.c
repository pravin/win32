/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 *
 * File: selection.c
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "selection.h"

#define SELECTED(m)     ((m) >= 128)

void
Sel_Clear(Selection *sel)
{
    free(sel->mask);
    sel->mask = NULL;
    SetRectEmpty(&sel->bounds);
}

BOOL
Sel_IsActive(const Selection *sel)
{
    return sel->mask != NULL;
}

BOOL
Sel_IsSelected(const Selection *sel, int w, int x, int y)
{
    if (!sel->mask)
        return TRUE;
    return SELECTED(sel->mask[(size_t)y * w + x]);
}

BOOL
Sel_SelectAll(Selection *sel, int w, int h)
{
    uint8_t *mask = (uint8_t *)malloc((size_t)w * h);
    if (!mask)
        return FALSE;
    memset(mask, 255, (size_t)w * h);
    Sel_Clear(sel);
    sel->mask = mask;
    SetRect(&sel->bounds, 0, 0, w, h);
    return TRUE;
}

BOOL
Sel_Invert(Selection *sel, int w, int h)
{
    size_t i, count = (size_t)w * h;

    if (!sel->mask)
        return Sel_SelectAll(sel, w, h);
    for (i = 0; i < count; i++)
        sel->mask[i] = (uint8_t)(255 - sel->mask[i]);
    Sel_RecalcBounds(sel, w, h);
    return TRUE;
}

uint8_t *
Sel_CloneMask(const Selection *sel, int w, int h)
{
    uint8_t *copy;
    if (!sel->mask)
        return NULL;
    copy = (uint8_t *)malloc((size_t)w * h);
    if (copy)
        memcpy(copy, sel->mask, (size_t)w * h);
    return copy;
}

BOOL
Sel_SetMask(Selection *sel, int w, int h, const uint8_t *mask)
{
    uint8_t *copy = NULL;

    if (mask)
    {
        copy = (uint8_t *)malloc((size_t)w * h);
        if (!copy)
            return FALSE;
        memcpy(copy, mask, (size_t)w * h);
    }
    Sel_Clear(sel);
    sel->mask = copy;
    Sel_RecalcBounds(sel, w, h);
    return TRUE;
}

void
Sel_RecalcBounds(Selection *sel, int w, int h)
{
    int x, y;
    RECT b = { w, h, 0, 0 };

    if (!sel->mask)
    {
        SetRectEmpty(&sel->bounds);
        return;
    }
    for (y = 0; y < h; y++)
    {
        const uint8_t *row = sel->mask + (size_t)y * w;
        for (x = 0; x < w; x++)
        {
            if (row[x])
            {
                if (x < b.left)
                    b.left = x;
                if (x + 1 > b.right)
                    b.right = x + 1;
                if (y < b.top)
                    b.top = y;
                if (y + 1 > b.bottom)
                    b.bottom = y + 1;
            }
        }
    }
    if (b.right <= b.left || b.bottom <= b.top)
        Sel_Clear(sel);         // Nothing is selected, so there is no selection
    else
        sel->bounds = b;
}

// Function: Sel_ClipBounds
// Trims a rectangle to the image. Returns FALSE if nothing is left.
static BOOL
Sel_ClipBounds(RECT *r, int w, int h)
{
    r->left = max(r->left, 0);
    r->top = max(r->top, 0);
    r->right = min(r->right, w);
    r->bottom = min(r->bottom, h);
    return r->right > r->left && r->bottom > r->top;
}

uint8_t *
Sel_ShapeRect(int x0, int y0, int x1, int y1, int w, int h, RECT *bounds)
{
    uint8_t *shape;

    // Corners in any order; the rectangle includes both end pixels
    bounds->left = min(x0, x1);
    bounds->top = min(y0, y1);
    bounds->right = max(x0, x1) + 1;
    bounds->bottom = max(y0, y1) + 1;
    if (!Sel_ClipBounds(bounds, w, h))
        return NULL;
    shape = (uint8_t *)malloc((size_t)(bounds->right - bounds->left) * (bounds->bottom - bounds->top));
    if (shape)
        memset(shape, 255, (size_t)(bounds->right - bounds->left) * (bounds->bottom - bounds->top));
    return shape;
}

uint8_t *
Sel_ShapeEllipse(int x0, int y0, int x1, int y1, int w, int h, RECT *bounds)
{
    RECT full;
    double cx, cy, rx, ry;
    uint8_t *shape;
    int bw, x, y;

    full.left = min(x0, x1);
    full.top = min(y0, y1);
    full.right = max(x0, x1) + 1;
    full.bottom = max(y0, y1) + 1;
    cx = (full.left + full.right) / 2.0;
    cy = (full.top + full.bottom) / 2.0;
    rx = (full.right - full.left) / 2.0;
    ry = (full.bottom - full.top) / 2.0;

    *bounds = full;
    if (!Sel_ClipBounds(bounds, w, h))
        return NULL;
    bw = bounds->right - bounds->left;
    shape = (uint8_t *)calloc((size_t)bw * (bounds->bottom - bounds->top), 1);
    if (!shape)
        return NULL;

    // A pixel is inside if its centre is inside the ellipse: (dx/rx)^2 + (dy/ry)^2 <= 1
    for (y = bounds->top; y < bounds->bottom; y++)
    {
        for (x = bounds->left; x < bounds->right; x++)
        {
            double dx = (x + 0.5 - cx) / rx;
            double dy = (y + 0.5 - cy) / ry;
            if (dx * dx + dy * dy <= 1.0)
                shape[(size_t)(y - bounds->top) * bw + (x - bounds->left)] = 255;
        }
    }
    return shape;
}

// Function: Sel_CompareDouble
static int
Sel_CompareDouble(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

uint8_t *
Sel_ShapePolygon(const POINT *points, int count, int w, int h, RECT *bounds)
{
    RECT full = { points[0].x, points[0].y, points[0].x + 1, points[0].y + 1 };
    uint8_t *shape;
    double *xs;
    int bw, i, y;

    for (i = 1; i < count; i++)
    {
        full.left = min(full.left, points[i].x);
        full.top = min(full.top, points[i].y);
        full.right = max(full.right, points[i].x + 1);
        full.bottom = max(full.bottom, points[i].y + 1);
    }
    *bounds = full;
    if (count < 3 || !Sel_ClipBounds(bounds, w, h))
        return NULL;
    bw = bounds->right - bounds->left;
    shape = (uint8_t *)calloc((size_t)bw * (bounds->bottom - bounds->top), 1);
    xs = (double *)malloc((size_t)count * sizeof(double));
    if (!shape || !xs)
    {
        free(shape);
        free(xs);
        return NULL;
    }

    // Scanline fill. For each row, look at the line through the middle of the
    // pixels, find where the outline crosses it, sort the crossings, and fill
    // between the first and second, the third and fourth, and so on. This is
    // the "even odd" rule: inside is anywhere you cross an odd number of edges.
    for (y = bounds->top; y < bounds->bottom; y++)
    {
        double yc = y + 0.5;
        int n = 0, k;

        for (i = 0; i < count; i++)
        {
            POINT a = points[i], b = points[(i + 1) % count];
            double ya = a.y + 0.5, yb = b.y + 0.5;

            // Does this edge cross the row? (Half open, so a corner is not counted twice.)
            if ((ya <= yc && yb > yc) || (yb <= yc && ya > yc))
                xs[n++] = a.x + 0.5 + (yc - ya) * (b.x - a.x) / (yb - ya);
        }
        qsort(xs, (size_t)n, sizeof(double), Sel_CompareDouble);
        for (k = 0; k + 1 < n; k += 2)
        {
            int xa = (int)ceil(xs[k] - 0.5), xb = (int)floor(xs[k + 1] - 0.5);
            int x;
            xa = max(xa, bounds->left);
            xb = min(xb, bounds->right - 1);
            for (x = xa; x <= xb; x++)
                shape[(size_t)(y - bounds->top) * bw + (x - bounds->left)] = 255;
        }
    }
    free(xs);
    return shape;
}

// Function: Sel_Mix
// What one pixel becomes. base is how selected it was before the drag, shape is
// how much the shape covers it.
static uint8_t
Sel_Mix(SelMode mode, uint8_t base, uint8_t shape)
{
    switch (mode)
    {
    case SEL_ADD:
        return base > shape ? base : shape;
    case SEL_SUBTRACT:
        return (uint8_t)(base * (255 - shape) / 255);
    default:
        return shape;
    }
}

void
Sel_Combine(Selection *sel, int w, int h, const uint8_t *base, SelMode mode,
    const uint8_t *shape, const RECT *shapeBounds, RECT *region)
{
    RECT all, area;
    int x, y, sw = 0;

    if (shape && shapeBounds)
    {
        UnionRect(&all, region, shapeBounds);
        *region = all;
        sw = shapeBounds->right - shapeBounds->left;
    }
    area = *region;
    if (!Sel_ClipBounds(&area, w, h))
        return;

    for (y = area.top; y < area.bottom; y++)
    {
        for (x = area.left; x < area.right; x++)
        {
            size_t i = (size_t)y * w + x;
            uint8_t b = base ? base[i] : 0;
            uint8_t s = 0;

            if (shape && x >= shapeBounds->left && x < shapeBounds->right
                && y >= shapeBounds->top && y < shapeBounds->bottom)
                s = shape[(size_t)(y - shapeBounds->top) * sw + (x - shapeBounds->left)];
            sel->mask[i] = Sel_Mix(mode, b, s);
        }
    }
}
