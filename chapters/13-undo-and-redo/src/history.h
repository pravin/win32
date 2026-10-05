/* DrawLite - Win32 Tutorial
 * Chapter 13 - Undo and redo
 *
 * File: history.h
 * Remembers what the user has done, so that they can take it back.
 *
 * We do not store "what the tool did". We store pictures. Every time an edit
 * finishes, we keep a copy of the pixels in the rectangle it changed, from
 * before and from after. Undo copies the "before" pixels back. Redo copies the
 * "after" pixels. It does not matter whether the change was a pencil dot or a
 * flood fill, and it will not matter for any tool we write later either.
 */
#ifndef HISTORY_H
#define HISTORY_H

#include <windows.h>
#include <stdint.h>
#include "surface.h"

// Struct: HistoryItem
// One undoable change.
typedef struct HistoryItem
{
    RECT area;          // The part of the surface that changed
    uint32_t *before;   // Its pixels before the change, row by row
    uint32_t *after;    // Its pixels after
    size_t bytes;       // Memory used by both copies
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

// Function: History_Push
// Records a change. "base" is the surface as it was before, "current" is the
// surface as it is now, and "area" is the part that is different. Anything the
// user had undone is thrown away, because you cannot redo after doing something new.
//
// Returns:
//   TRUE if it was recorded. FALSE if we ran out of memory, in which case the
//   history is cleared (so we never undo to the wrong place).
BOOL History_Push(History *h, const Surface *base, const Surface *current, const RECT *area);

// Function: History_CanUndo
BOOL History_CanUndo(const History *h);

// Function: History_CanRedo
BOOL History_CanRedo(const History *h);

// Function: History_Undo
// Puts the previous pixels back on the surface. *area is set to the part that changed.
// Returns FALSE if there is nothing to undo.
BOOL History_Undo(History *h, Surface *s, RECT *area);

// Function: History_Redo
// The opposite of undo.
BOOL History_Redo(History *h, Surface *s, RECT *area);

#endif // HISTORY_H
