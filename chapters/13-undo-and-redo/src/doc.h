/* DrawLite - Win32 Tutorial
 * Chapter 13 - Undo and redo
 *
 * File: doc.h
 * The picture you are working on.
 */
#ifndef DOC_H
#define DOC_H

#include <windows.h>
#include "surface.h"
#include "history.h"

// Struct: Document
typedef struct Document
{
    int width;
    int height;
    Surface *surface;       // The pixels
    History *history;       // Everything that can be undone and redone
    BOOL modified;          // Has it changed since it was last saved?
    wchar_t path[MAX_PATH]; // File it was loaded from or saved to. Empty if none.
} Document;

// Function: Doc_Create
// Creates a document with a white background. Returns NULL on failure.
Document *Doc_Create(int width, int height);

// Function: Doc_Destroy
// Frees a document and everything inside it. Safe to call with NULL.
void Doc_Destroy(Document *doc);

#endif // DOC_H
