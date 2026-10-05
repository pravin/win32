/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 *
 * File: doc.c
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "doc.h"
#include "composite.h"
#include "history.h"
#include "pixel.h"

Layer *
Layer_Create(int width, int height, const wchar_t *name)
{
    Layer *layer = (Layer *)calloc(1, sizeof(Layer));
    if (!layer)
        return NULL;
    layer->surface = Surface_Create(width, height);
    if (!layer->surface)
    {
        free(layer);
        return NULL;
    }
    StringCchCopyW(layer->name, LAYER_NAME_MAX, name);
    layer->visible = TRUE;
    layer->opacity = 255;
    return layer;
}

Layer *
Layer_Clone(const Layer *layer)
{
    Layer *copy = (Layer *)calloc(1, sizeof(Layer));
    if (!copy)
        return NULL;
    copy->surface = Surface_Clone(layer->surface);
    if (!copy->surface)
    {
        free(copy);
        return NULL;
    }
    Layer_CopyProps(copy, layer);
    return copy;
}

void
Layer_Destroy(Layer *layer)
{
    if (!layer)
        return;
    Surface_Destroy(layer->surface);
    free(layer);
}

void
Layer_CopyProps(Layer *dst, const Layer *src)
{
    StringCchCopyW(dst->name, LAYER_NAME_MAX, src->name);
    dst->visible = src->visible;
    dst->opacity = src->opacity;
    dst->blend = src->blend;
}

Document *
Doc_CreateFromSurface(Surface *surface)
{
    Document *doc = (Document *)calloc(1, sizeof(Document));
    Layer *layer = NULL;

    if (!doc)
        return NULL;

    // The layer takes over the surface we were given
    layer = (Layer *)calloc(1, sizeof(Layer));
    if (!layer)
    {
        free(doc);
        return NULL;
    }
    layer->surface = surface;
    StringCchCopyW(layer->name, LAYER_NAME_MAX, L"Background");
    layer->visible = TRUE;
    layer->opacity = 255;

    doc->width = surface->width;
    doc->height = surface->height;
    doc->view = Surface_Create(doc->width, doc->height);
    // Undo may use up to 256 MB before it starts forgetting the oldest changes
    doc->history = History_Create(256u * 1024 * 1024);
    if (!doc->view || !doc->history || !Doc_InsertLayer(doc, 0, layer))
    {
        // Careful: the surface is still the caller's, so detach it before freeing the layer
        layer->surface = NULL;
        Layer_Destroy(layer);
        Surface_Destroy(doc->view);
        History_Destroy(doc->history);
        free(doc->layers);
        free(doc);
        return NULL;
    }
    return doc;
}

Document *
Doc_Create(int width, int height)
{
    Surface *s = Surface_Create(width, height);
    Document *doc;

    if (!s)
        return NULL;
    Surface_Fill(s, PIX_WHITE);
    doc = Doc_CreateFromSurface(s);
    if (!doc)
        Surface_Destroy(s);
    return doc;
}

void
Doc_Destroy(Document *doc)
{
    int i;

    if (!doc)
        return;
    History_Destroy(doc->history);
    for (i = 0; i < doc->layerCount; i++)
        Layer_Destroy(doc->layers[i]);
    free(doc->layers);
    Surface_Destroy(doc->view);
    Sel_Clear(&doc->sel);
    free(doc);
}

Layer *
Doc_ActiveLayer(const Document *doc)
{
    return doc->layers[doc->active];
}

Surface *
Doc_ActiveSurface(const Document *doc)
{
    return doc->layers[doc->active]->surface;
}

BOOL
Doc_InsertLayer(Document *doc, int index, Layer *layer)
{
    if (index < 0 || index > doc->layerCount)
        return FALSE;
    if (doc->layerCount == doc->layerCapacity)
    {
        int newCap = doc->layerCapacity ? doc->layerCapacity * 2 : 8;
        Layer **bigger = (Layer **)realloc(doc->layers, (size_t)newCap * sizeof(Layer *));
        if (!bigger)
            return FALSE;
        doc->layers = bigger;
        doc->layerCapacity = newCap;
    }
    // Shuffle the layers above the new one up a place
    memmove(&doc->layers[index + 1], &doc->layers[index], (size_t)(doc->layerCount - index) * sizeof(Layer *));
    doc->layers[index] = layer;
    doc->layerCount++;
    doc->active = index;
    Doc_UpdateAllOfView(doc);
    return TRUE;
}

Layer *
Doc_RemoveLayer(Document *doc, int index)
{
    Layer *layer;

    if (index < 0 || index >= doc->layerCount)
        return NULL;
    layer = doc->layers[index];
    memmove(&doc->layers[index], &doc->layers[index + 1], (size_t)(doc->layerCount - index - 1) * sizeof(Layer *));
    doc->layerCount--;

    // Keep the same layer active if we can. If we removed the active one, the one below.
    if (index <= doc->active && doc->active > 0)
        doc->active--;
    Doc_UpdateAllOfView(doc);
    return layer;
}

void
Doc_MoveLayer(Document *doc, int from, int to)
{
    Layer *layer;

    if (from < 0 || from >= doc->layerCount || to < 0 || to >= doc->layerCount || from == to)
        return;
    layer = doc->layers[from];
    if (from < to)
        memmove(&doc->layers[from], &doc->layers[from + 1], (size_t)(to - from) * sizeof(Layer *));
    else
        memmove(&doc->layers[to + 1], &doc->layers[to], (size_t)(from - to) * sizeof(Layer *));
    doc->layers[to] = layer;
    doc->active = to;
    Doc_UpdateAllOfView(doc);
}
