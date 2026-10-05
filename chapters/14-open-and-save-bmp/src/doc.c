/* DrawLite - Win32 Tutorial
 * Chapter 14 - Open and save
 *
 * File: doc.c
 */

#include <stdlib.h>
#include "doc.h"
#include "pixel.h"

Document *
Doc_CreateFromSurface(Surface *surface)
{
    Document *doc = (Document *)calloc(1, sizeof(Document));
    if (!doc)
        return NULL;

    doc->surface = surface;
    // Undo may use up to 256 MB before it starts forgetting the oldest changes
    doc->history = History_Create(256u * 1024 * 1024);
    if (!doc->history)
    {
        free(doc);
        return NULL;
    }
    doc->width = surface->width;
    doc->height = surface->height;
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
    if (!doc)
        return;
    History_Destroy(doc->history);
    Surface_Destroy(doc->surface);
    free(doc);
}
