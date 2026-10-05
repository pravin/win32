/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 *
 * File: history.c
 */

#include <stdlib.h>
#include <string.h>
#include "history.h"
#include "composite.h"

History *
History_Create(size_t limitBytes)
{
    History *h = (History *)calloc(1, sizeof(History));
    if (h)
        h->limit = limitBytes;
    return h;
}

static void
HistoryItem_Free(HistoryItem *item)
{
    free(item->before);
    free(item->after);
    free(item->selBefore);
    free(item->selAfter);
    Layer_Destroy(item->held);      // Only set while the layer is out of the document
    ZeroMemory(item, sizeof(*item));
}

// Function: History_DropFrom
// Frees every item from index "first" onwards.
static void
History_DropFrom(History *h, int first)
{
    int i;
    for (i = first; i < h->count; i++)
    {
        h->bytes -= h->items[i].bytes;
        HistoryItem_Free(&h->items[i]);
    }
    h->count = first;
    if (h->position > first)
        h->position = first;
}

void
History_Clear(History *h)
{
    History_DropFrom(h, 0);
}

void
History_Destroy(History *h)
{
    if (!h)
        return;
    History_Clear(h);
    free(h->items);
    free(h);
}

// Function: History_CopyRect
// Copies the pixels inside "area" from a surface into a tightly packed array.
static uint32_t *
History_CopyRect(const Surface *s, const RECT *area)
{
    int w = area->right - area->left;
    int h = area->bottom - area->top;
    uint32_t *copy = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
    int y;

    if (!copy)
        return NULL;
    for (y = 0; y < h; y++)
        memcpy(copy + (size_t)y * w, s->pixels + (size_t)(area->top + y) * s->width + area->left,
            (size_t)w * sizeof(uint32_t));
    return copy;
}

// Function: History_PasteRect
// The opposite: puts a packed array back into the surface.
static void
History_PasteRect(Surface *s, const RECT *area, const uint32_t *pixels)
{
    int w = area->right - area->left;
    int h = area->bottom - area->top;
    int y;

    for (y = 0; y < h; y++)
        memcpy(s->pixels + (size_t)(area->top + y) * s->width + area->left,
            pixels + (size_t)y * w, (size_t)w * sizeof(uint32_t));
}

// Function: History_Add
// Adds a finished item to the end of the list. Takes over its memory, and
// frees it if there is no room.
static BOOL
History_Add(History *h, HistoryItem *item)
{
    // A new change means the redo list is no longer valid
    History_DropFrom(h, h->position);

    if (h->count == h->capacity)
    {
        int newCap = h->capacity ? h->capacity * 2 : 32;
        HistoryItem *bigger = (HistoryItem *)realloc(h->items, (size_t)newCap * sizeof(HistoryItem));
        if (!bigger)
        {
            HistoryItem_Free(item);
            History_Clear(h);
            return FALSE;
        }
        h->items = bigger;
        h->capacity = newCap;
    }
    h->items[h->count++] = *item;
    h->position = h->count;
    h->bytes += item->bytes;

    // Too much memory? Forget the oldest changes. We always keep the newest one.
    while (h->bytes > h->limit && h->count > 1)
    {
        h->bytes -= h->items[0].bytes;
        HistoryItem_Free(&h->items[0]);
        memmove(&h->items[0], &h->items[1], (size_t)(h->count - 1) * sizeof(HistoryItem));
        h->count--;
        h->position--;
    }
    return TRUE;
}

BOOL
History_PushPixels(History *h, int layer, const Surface *base, const Surface *current, const RECT *area)
{
    HistoryItem item;
    RECT clip = { 0, 0, current->width, current->height };

    ZeroMemory(&item, sizeof(item));
    if (!IntersectRect(&item.area, area, &clip))
        return TRUE; // Nothing changed inside the surface, nothing to remember

    item.kind = HIST_PIXELS;
    item.layer = layer;
    item.before = History_CopyRect(base, &item.area);
    item.after = History_CopyRect(current, &item.area);
    if (!item.before || !item.after)
    {
        HistoryItem_Free(&item);
        History_Clear(h);
        return FALSE;
    }
    item.bytes = 2 * (size_t)(item.area.right - item.area.left) * (item.area.bottom - item.area.top) * sizeof(uint32_t);
    return History_Add(h, &item);
}

// Function: History_LayerBytes
// A layer on the shelf costs us its whole surface.
static size_t
History_LayerBytes(const Layer *layer)
{
    return (size_t)layer->surface->width * layer->surface->height * sizeof(uint32_t);
}

BOOL
History_PushLayerAdded(History *h, int index)
{
    HistoryItem item;

    ZeroMemory(&item, sizeof(item));
    item.kind = HIST_LAYER_ADDED;
    item.layer = index;
    return History_Add(h, &item);
}

BOOL
History_PushLayerRemoved(History *h, Layer *removed, int index)
{
    HistoryItem item;

    ZeroMemory(&item, sizeof(item));
    item.kind = HIST_LAYER_REMOVED;
    item.layer = index;
    item.held = removed;
    item.bytes = History_LayerBytes(removed);
    return History_Add(h, &item);   // Frees the layer if it fails
}

BOOL
History_PushLayerMoved(History *h, int from, int to)
{
    HistoryItem item;

    ZeroMemory(&item, sizeof(item));
    item.kind = HIST_LAYER_MOVED;
    item.layer = from;
    item.layerTo = to;
    return History_Add(h, &item);
}

BOOL
History_PushLayerProps(History *h, int index, const Layer *before, const Layer *after)
{
    HistoryItem item;

    ZeroMemory(&item, sizeof(item));
    item.kind = HIST_LAYER_PROPS;
    item.layer = index;
    Layer_CopyProps(&item.propsBefore, before);
    Layer_CopyProps(&item.propsAfter, after);
    return History_Add(h, &item);
}

BOOL
History_PushSelection(History *h, uint8_t *before, uint8_t *after, int w, int hgt)
{
    HistoryItem item;

    ZeroMemory(&item, sizeof(item));
    item.kind = HIST_SELECTION;
    item.selBefore = before;
    item.selAfter = after;
    item.bytes = (before ? (size_t)w * hgt : 0) + (after ? (size_t)w * hgt : 0);
    return History_Add(h, &item);
}

BOOL
History_CanUndo(const History *h)
{
    return h->position > 0;
}

BOOL
History_CanRedo(const History *h)
{
    return h->position < h->count;
}

// Function: History_Apply
// Does one item in one direction. "forward" is TRUE for redo, FALSE for undo.
// Because Add and Remove are mirror images, we only need to say whether this
// step puts a layer into the stack or takes one out.
static void
History_Apply(HistoryItem *item, Document *doc, BOOL forward, RECT *area)
{
    // The whole picture changes whenever the stack changes
    SetRect(area, 0, 0, doc->width, doc->height);

    switch (item->kind)
    {
    case HIST_PIXELS:
    {
        Surface *s = doc->layers[item->layer]->surface;
        History_PasteRect(s, &item->area, forward ? item->after : item->before);
        doc->active = item->layer;
        *area = item->area;
        Doc_UpdateView(doc, area);
        break;
    }
    case HIST_LAYER_ADDED:
    case HIST_LAYER_REMOVED:
    {
        // Redoing an add inserts. Undoing a remove inserts. The others take out.
        BOOL insert = (item->kind == HIST_LAYER_ADDED) == forward;
        if (insert)
        {
            Doc_InsertLayer(doc, item->layer, item->held);
            item->held = NULL;
        }
        else
            item->held = Doc_RemoveLayer(doc, item->layer);
        break;
    }
    case HIST_LAYER_MOVED:
        if (forward)
            Doc_MoveLayer(doc, item->layer, item->layerTo);
        else
            Doc_MoveLayer(doc, item->layerTo, item->layer);
        break;
    case HIST_SELECTION:
        Sel_SetMask(&doc->sel, doc->width, doc->height, forward ? item->selAfter : item->selBefore);
        break;
    case HIST_LAYER_PROPS:
        Layer_CopyProps(doc->layers[item->layer], forward ? &item->propsAfter : &item->propsBefore);
        doc->active = item->layer;
        Doc_UpdateAllOfView(doc);
        break;
    }
}

BOOL
History_Undo(History *h, Document *doc, RECT *area)
{
    if (!History_CanUndo(h))
        return FALSE;
    History_Apply(&h->items[--h->position], doc, FALSE, area);
    return TRUE;
}

BOOL
History_Redo(History *h, Document *doc, RECT *area)
{
    if (!History_CanRedo(h))
        return FALSE;
    History_Apply(&h->items[h->position++], doc, TRUE, area);
    return TRUE;
}
