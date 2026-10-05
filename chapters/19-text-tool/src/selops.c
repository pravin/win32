/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: selops.c
 */

#include <stdlib.h>
#include "selops.h"
#include "composite.h"
#include "history.h"
#include "pixel.h"

Surface *
SelOps_CopyPixels(const Document *doc)
{
    const Selection *sel = &doc->sel;
    const Surface *src = Doc_ActiveSurface(doc);
    Surface *out;
    int bw, bh, x, y;

    if (!sel->mask)
        return NULL;
    bw = sel->bounds.right - sel->bounds.left;
    bh = sel->bounds.bottom - sel->bounds.top;
    out = Surface_Create(bw, bh);
    if (!out)
        return NULL;

    for (y = 0; y < bh; y++)
    {
        for (x = 0; x < bw; x++)
        {
            size_t i = (size_t)(sel->bounds.top + y) * doc->width + (sel->bounds.left + x);
            // How selected a pixel is says how much of it we keep
            out->pixels[(size_t)y * bw + x] = Pixel_Scale(src->pixels[i], sel->mask[i]);
        }
    }
    return out;
}

BOOL
SelOps_ClearPixels(Document *doc)
{
    const Selection *sel = &doc->sel;
    Surface *layer = Doc_ActiveSurface(doc);
    Surface *before;
    RECT area;
    int x, y;

    if (!sel->mask)
        return FALSE;
    area = sel->bounds;

    // The history wants to know how things looked before, so keep a copy
    before = Surface_Clone(layer);
    if (!before)
        return FALSE;

    for (y = area.top; y < area.bottom; y++)
    {
        for (x = area.left; x < area.right; x++)
        {
            size_t i = (size_t)y * doc->width + x;
            // Keep (255 - mask) of the pixel: all of it if unselected, none if fully selected
            if (sel->mask[i])
                layer->pixels[i] = Pixel_Scale(layer->pixels[i], 255 - sel->mask[i]);
        }
    }
    History_PushPixels(doc->history, doc->active, before, layer, &area);
    Surface_Destroy(before);
    Doc_UpdateView(doc, &area);
    doc->modified = TRUE;
    return TRUE;
}
