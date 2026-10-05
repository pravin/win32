/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: composite.c
 */

#include <stdlib.h>
#include "composite.h"
#include "blend.h"
#include "pixel.h"

// The two colours of the chequerboard and the size of its squares
#define CHECK_LIGHT     0xFFFFFFFFu
#define CHECK_DARK      0xFFCCCCCCu
#define CHECK_SIZE      8

// Function: Layer_PixelAt
// The pixel of a layer at index i, with the layer's opacity applied.
static inline uint32_t
Layer_PixelAt(const Layer *layer, size_t i)
{
    uint32_t p = layer->surface->pixels[i];
    if (layer->opacity < 255)
        p = Pixel_Scale(p, (uint32_t)layer->opacity);
    return p;
}

// Function: Doc_PixelIndex
// Stack up the visible layers for one pixel, bottom to top.
static uint32_t
Doc_PixelIndex(const Document *doc, size_t i)
{
    uint32_t acc = 0;   // Start with nothing: fully see-through
    int l;

    for (l = 0; l < doc->layerCount; l++)
    {
        const Layer *layer = doc->layers[l];
        if (layer->visible)
            acc = Blend_Pixel(layer->blend, Layer_PixelAt(layer, i), acc);
    }
    return acc;
}

void
Doc_UpdateView(Document *doc, const RECT *area)
{
    RECT r;
    int x, y;

    r.left = max(area->left, 0);
    r.top = max(area->top, 0);
    r.right = min(area->right, doc->width);
    r.bottom = min(area->bottom, doc->height);

    for (y = r.top; y < r.bottom; y++)
    {
        for (x = r.left; x < r.right; x++)
        {
            size_t i = (size_t)y * doc->width + x;
            uint32_t check = (((x / CHECK_SIZE) + (y / CHECK_SIZE)) & 1) ? CHECK_DARK : CHECK_LIGHT;
            doc->view->pixels[i] = Pixel_Over(Doc_PixelIndex(doc, i), check);
        }
    }
}

void
Doc_UpdateAllOfView(Document *doc)
{
    RECT all;
    SetRect(&all, 0, 0, doc->width, doc->height);
    Doc_UpdateView(doc, &all);
}

Surface *
Doc_Flatten(const Document *doc)
{
    Surface *s = Surface_Create(doc->width, doc->height);
    size_t i, count = (size_t)doc->width * doc->height;

    if (!s)
        return NULL;
    for (i = 0; i < count; i++)
        s->pixels[i] = Doc_PixelIndex(doc, i);
    return s;
}

uint32_t
Doc_PixelAt(const Document *doc, int x, int y)
{
    if (x < 0 || y < 0 || x >= doc->width || y >= doc->height)
        return 0;
    return Doc_PixelIndex(doc, (size_t)y * doc->width + x);
}
