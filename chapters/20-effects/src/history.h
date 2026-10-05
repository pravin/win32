/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: history.h
 * Remembers what the user has done, so that they can take it back.
 *
 * Most changes are pictures. Every time a tool finishes, we keep a copy of
 * the pixels in the rectangle it changed, from before and from after. Undo
 * copies the "before" pixels back. Redo copies the "after" pixels.
 *
 * Layers add changes that are not pixels: a layer was added, deleted, moved,
 * or had its settings changed. Each kind of change knows how to undo itself.
 */
#ifndef HISTORY_H
#define HISTORY_H

#include <windows.h>
#include <stdint.h>
#include "doc.h"

// Enum: HistoryKind
// What sort of change an item records.
typedef enum HistoryKind
{
    HIST_PIXELS,        // A tool changed pixels on a layer
    HIST_LAYER_ADDED,   // A layer was put in the stack
    HIST_LAYER_REMOVED, // A layer was taken out of the stack
    HIST_LAYER_MOVED,   // A layer changed places
    HIST_LAYER_PROPS,   // A layer's name, visibility or opacity changed
    HIST_SELECTION      // The selection changed
} HistoryKind;

// Struct: HistoryItem
// One undoable change. Which fields are used depends on the kind.
typedef struct HistoryItem
{
    HistoryKind kind;
    int layer;          // The layer it happened to (HIST_LAYER_MOVED: where it started)
    int layerTo;        // HIST_LAYER_MOVED: where it ended up
    RECT area;          // HIST_PIXELS: the part of the layer that changed
    uint32_t *before;   // HIST_PIXELS: its pixels before the change, row by row
    uint32_t *after;    // HIST_PIXELS: its pixels after
    Layer *held;        // ADDED/REMOVED: the layer, while it is NOT in the document
    Layer propsBefore;  // HIST_LAYER_PROPS: the settings before (the surface pointer is unused)
    Layer propsAfter;
    uint8_t *selBefore; // HIST_SELECTION: the selection mask before (NULL: nothing was selected)
    uint8_t *selAfter;  // and after
    size_t bytes;       // Memory this item uses, so we can keep the total in check
} HistoryItem;

// Struct: History
// The list of changes, and how far along it we are.
//
//   items:  [ 0 ][ 1 ][ 2 ][ 3 ][ 4 ]
//                        ^
//                        position = 3
//
// Items 0, 1 and 2 have been done. 3 and 4 have been undone and can be redone.
typedef struct History
{
    HistoryItem *items;
    int count;          // How many items we hold
    int capacity;       // How many we have room for
    int position;       // Number of items that are currently "done"
    size_t bytes;       // Memory used by all items
    size_t limit;       // When bytes goes over this, we forget the oldest items
} History;

// Function: History_Create
// Makes an empty history that will use at most about limitBytes of memory.
History *History_Create(size_t limitBytes);

// Function: History_Destroy
// Frees a history. Safe to call with NULL.
void History_Destroy(History *h);

// Function: History_Clear
// Forgets everything.
void History_Clear(History *h);

// Function: History_PushPixels
// Records a change to the pixels of one layer. "base" is the surface as it
// was before, "current" is the surface as it is now, and "area" is the part
// that is different. Anything the user had undone is thrown away, because you
// cannot redo after doing something new.
//
// Returns:
//   TRUE if it was recorded. FALSE if we ran out of memory, in which case the
//   history is cleared (so we never undo to the wrong place).
BOOL History_PushPixels(History *h, int layer, const Surface *base, const Surface *current, const RECT *area);

// Function: History_PushLayerAdded
// Records that a layer was just inserted at "index".
BOOL History_PushLayerAdded(History *h, int index);

// Function: History_PushLayerRemoved
// Records that a layer was just taken out of the stack from "index". The
// history takes over the layer (you must not free it), and gives it back when
// the removal is undone. If this fails the layer is freed for you.
BOOL History_PushLayerRemoved(History *h, Layer *removed, int index);

// Function: History_PushLayerMoved
// Records that a layer was moved from one place in the stack to another.
BOOL History_PushLayerMoved(History *h, int from, int to);

// Function: History_PushLayerProps
// Records that a layer's settings changed. "before" and "after" are copies of the
// layer taken before and after the change. Only the settings are used.
BOOL History_PushLayerProps(History *h, int index, const Layer *before, const Layer *after);

// Function: History_PushSelection
// Records a change of selection. The history takes over both masks (free()d
// when forgotten) which are w * h bytes, or NULL for "nothing selected".
// If this fails the masks are freed for you.
BOOL History_PushSelection(History *h, uint8_t *before, uint8_t *after, int w, int hgt);

// Function: History_CanUndo
BOOL History_CanUndo(const History *h);

// Function: History_CanRedo
BOOL History_CanRedo(const History *h);

// Function: History_Undo
// Takes back the last change. *area is set to the part of the image that
// needs repainting. Returns FALSE if there is nothing to undo.
BOOL History_Undo(History *h, Document *doc, RECT *area);

// Function: History_Redo
// The opposite of undo.
BOOL History_Redo(History *h, Document *doc, RECT *area);

#endif // HISTORY_H
