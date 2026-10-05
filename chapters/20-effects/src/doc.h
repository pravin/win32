/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: doc.h
 * The picture you are working on: a stack of layers.
 *
 * Think of layers as sheets of clear plastic stacked on top of each other,
 * each with something drawn on it. You draw on one sheet at a time. What you
 * see is all of them seen through each other. Layer 0 is the bottom sheet.
 */
#ifndef DOC_H
#define DOC_H

#include <windows.h>
#include "surface.h"
#include "blend.h"
#include "selection.h"

#define LAYER_NAME_MAX  32

// Struct: Layer
// One sheet of the stack.
typedef struct Layer
{
    Surface *surface;               // The pixels. Same size as the document.
    wchar_t name[LAYER_NAME_MAX];
    BOOL visible;                   // Hidden layers are skipped when drawing
    int opacity;                    // How solid the whole layer is: 0 (gone) to 255 (full)
    BlendMode blend;                // How it mixes with the layers below
} Layer;

// Forward declaration. history.h includes this file, so we cannot include it here.
typedef struct History History;

// Struct: Document
typedef struct Document
{
    int width;
    int height;
    Layer **layers;         // layers[0] is at the bottom
    int layerCount;
    int layerCapacity;
    int active;             // Index of the layer the tools draw on
    Selection sel;          // What is selected. sel.mask is NULL if nothing is.
    Surface *view;          // What the canvas shows: all visible layers over a chequerboard
    History *history;       // Everything that can be undone and redone
    BOOL modified;          // Has it changed since it was last saved?
    BOOL warnedFlat;        // Have we said that a flat file format loses the layers?
    wchar_t path[MAX_PATH]; // File it was loaded from or saved to. Empty if none.
} Document;

// Function: Layer_Create
// Makes a see-through layer. Returns NULL if out of memory.
Layer *Layer_Create(int width, int height, const wchar_t *name);

// Function: Layer_Clone
// Makes an exact copy of a layer, pixels and all.
Layer *Layer_Clone(const Layer *layer);

// Function: Layer_Destroy
// Frees a layer. Safe to call with NULL.
void Layer_Destroy(Layer *layer);

// Function: Layer_CopyProps
// Copies the name, visibility, opacity and blend mode (not the pixels) from one layer to another.
void Layer_CopyProps(Layer *dst, const Layer *src);

// Function: Doc_Create
// Creates a document with one white layer called "Background". Returns NULL on failure.
Document *Doc_Create(int width, int height);

// Function: Doc_CreateFromSurface
// Makes a one layer document around a surface you already have. The document
// takes over the surface and frees it when it is destroyed. Returns NULL on
// failure (and then the surface is still yours).
Document *Doc_CreateFromSurface(Surface *surface);

// Function: Doc_Destroy
// Frees a document and everything inside it. Safe to call with NULL.
void Doc_Destroy(Document *doc);

// Function: Doc_ActiveLayer
// The layer the tools draw on.
Layer *Doc_ActiveLayer(const Document *doc);

// Function: Doc_ActiveSurface
// The pixels of the active layer.
Surface *Doc_ActiveSurface(const Document *doc);

// Function: Doc_InsertLayer
// Puts a layer in the stack at "index" (0 is the bottom) and makes it the
// active layer. The document owns it from now on.
// Returns FALSE if out of memory (and the layer is still yours).
BOOL Doc_InsertLayer(Document *doc, int index, Layer *layer);

// Function: Doc_RemoveLayer
// Takes a layer out of the stack and gives it to you. You must free it, or put
// it back. The layer below becomes active.
Layer *Doc_RemoveLayer(Document *doc, int index);

// Function: Doc_MoveLayer
// Moves a layer from one position in the stack to another.
void Doc_MoveLayer(Document *doc, int from, int to);

#endif // DOC_H
