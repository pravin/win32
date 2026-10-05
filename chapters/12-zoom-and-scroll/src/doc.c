/* DrawLite - Win32 Tutorial
 * Chapter 12 - Zoom and scroll
 *
 * File: doc.c
 */

#include <stdlib.h>
#include "doc.h"
#include "pixel.h"

Document *
Doc_Create(int width, int height)
{
    Document *doc = (Document *)calloc(1, sizeof(Document));
    if (!doc)
        return NULL;

    doc->surface = Surface_Create(width, height);
    if (!doc->surface)
    {
        free(doc);
        return NULL;
    }
    doc->width = width;
    doc->height = height;
    Surface_Fill(doc->surface, PIX_WHITE);
    return doc;
}

void
Doc_Destroy(Document *doc)
{
    if (!doc)
        return;
    Surface_Destroy(doc->surface);
    free(doc);
}
