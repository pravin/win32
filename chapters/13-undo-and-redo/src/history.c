/* DrawLite - Win32 Tutorial
 * Chapter 13 - Undo and redo
 *
 * File: history.c
 */

#include <stdlib.h>
#include <string.h>
#include "history.h"

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

BOOL
History_Push(History *h, const Surface *base, const Surface *current, const RECT *area)
{
    HistoryItem item;
    RECT clip = { 0, 0, current->width, current->height };

    ZeroMemory(&item, sizeof(item));
    if (!IntersectRect(&item.area, area, &clip))
        return TRUE; // Nothing changed inside the surface, nothing to remember

    // A new change means the redo list is no longer valid
    History_DropFrom(h, h->position);

    item.before = History_CopyRect(base, &item.area);
    item.after = History_CopyRect(current, &item.area);
    if (!item.before || !item.after)
    {
        HistoryItem_Free(&item);
        History_Clear(h);
        return FALSE;
    }
    item.bytes = 2 * (size_t)(item.area.right - item.area.left) * (item.area.bottom - item.area.top) * sizeof(uint32_t);

    if (h->count == h->capacity)
    {
        int newCap = h->capacity ? h->capacity * 2 : 32;
        HistoryItem *bigger = (HistoryItem *)realloc(h->items, (size_t)newCap * sizeof(HistoryItem));
        if (!bigger)
        {
            HistoryItem_Free(&item);
            History_Clear(h);
            return FALSE;
        }
        h->items = bigger;
        h->capacity = newCap;
    }
    h->items[h->count++] = item;
    h->position = h->count;
    h->bytes += item.bytes;

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
History_CanUndo(const History *h)
{
    return h->position > 0;
}

BOOL
History_CanRedo(const History *h)
{
    return h->position < h->count;
}

BOOL
History_Undo(History *h, Surface *s, RECT *area)
{
    HistoryItem *item;

    if (!History_CanUndo(h))
        return FALSE;
    item = &h->items[--h->position];
    History_PasteRect(s, &item->area, item->before);
    *area = item->area;
    return TRUE;
}

BOOL
History_Redo(History *h, Surface *s, RECT *area)
{
    HistoryItem *item;

    if (!History_CanRedo(h))
        return FALSE;
    item = &h->items[h->position++];
    History_PasteRect(s, &item->area, item->after);
    *area = item->area;
    return TRUE;
}
