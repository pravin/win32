# Chapter 13 - Undo and redo

[< Chapter 12: Zoom and scroll](../12-zoom-and-scroll/README.md)

You have been drawing for three chapters without a safety net. Every slip of the mouse is permanent. Time to fix that. Undo and Redo have been sitting in the **Edit** menu and on the toolbar since the early chapters, greyed out and politely saying "Not written yet. Patience!" if you could reach them. Today they go live.

In this chapter you will

- keep a history of changes as before and after pictures of a rectangle,
- undo and redo with the menu, the toolbar and **Ctrl+Z** and **Ctrl+Y**,
- keep the memory the history uses under a limit,
- grey out the buttons when there is nothing to undo,
- learn why `GetCapture` is the wrong way to ask "is the mouse busy?".

## Before we begin

There are two new files this time, `src/history.c` and `src/history.h`. `CMakeLists.txt` lists them. The rest of the changes are in `doc.c`, `doc.h`, `tools.c`, `canvas.c`, `canvas.h` and `mainwindow.c`. Build and run as always (see [chapter 1](../01-hello-win32/README.md) if you need to).

```text
cmake -S . -B build && cmake --build build
```

Draw a few strokes, a rectangle and a fill. Then press Ctrl+Z and watch them disappear one at a time, newest first. Press Ctrl+Y to bring them back.

![The Edit menu with Undo and Redo enabled](images/screenshot.png)

Draw something new after an undo and the Redo button greys out again. That is how every program you have used works, and it is also how we do it.

## Breaking it up

### The big idea: remember pictures, not actions

There are two ways to build undo. You could record what the user did ("a red line from here to here, 8 pixels wide") and undo it by doing the opposite. That sounds neat, but it means every tool needs an "undo" version of itself, and the fill tool in particular has no obvious opposite.

The other way is much dumber and much better. Before each change, take a copy of the pixels that are about to change. After it, take another copy. To undo, put the "before" pixels back. To redo, put the "after" pixels back. It doesn't matter whether the change was a pencil dot or a flood fill. Nor will it matter for any tool we add later.

Think of a decorator repainting a patch of wall. Before she starts, she photographs the patch. When she finishes, she photographs it again. If the customer says "I preferred the old colour", she doesn't need to remember what paint she used. She pastes the first photo over the patch. If the customer changes their mind again, the second photo goes back on.

The patch is a rectangle, and we already know which one. Every `Edit` keeps `bounds`, the rectangle around everything it has touched, and `base`, a copy of the surface from before the change. It has done since chapter 8. Everything the history needs is already there.

### The data

```text
20  // Struct: HistoryItem
21  // One undoable change.
22  typedef struct HistoryItem
23  {
24      RECT area;          // The part of the surface that changed
25      uint32_t *before;   // Its pixels before the change, row by row
26      uint32_t *after;    // Its pixels after
27      size_t bytes;       // Memory used by both copies
28  } HistoryItem;
29
30  // Struct: History
31  // The list of changes, and how far along it we are.
32  //
33  //   items:  [ 0 ][ 1 ][ 2 ][ 3 ][ 4 ]
34  //                        ^
35  //                        position = 3
36  //
37  // Items 0, 1 and 2 have been done. 3 and 4 have been undone and can be redone.
38  typedef struct History
39  {
40      HistoryItem *items;
41      int count;          // How many items we hold
42      int capacity;       // How many we have room for
43      int position;       // Number of items that are currently "done"
44      size_t bytes;       // Memory used by all items
45      size_t limit;       // When bytes goes over this, we forget the oldest items
46  } History;
```

A `HistoryItem` is one undoable change. It has the rectangle, the `before` pixels and the `after` pixels, each stored tightly packed, row after row, with no gaps between rows. `bytes` is what the two copies cost in memory.

A `History` is a growing array of those items, plus a number called `position`. The picture in the comment shows the idea. Items to the left of the position have been done, and items to the right have been undone and could be redone. So undo means "step the position back one and paste that item's `before`", and redo means "paste that item's `after` and step the position forward".

### Copying a rectangle

Two small helpers do the copying.

```text
60  // Function: History_CopyRect
61  // Copies the pixels inside "area" from a surface into a tightly packed array.
62  static uint32_t *
63  History_CopyRect(const Surface *s, const RECT *area)
64  {
65      int w = area->right - area->left;
66      int h = area->bottom - area->top;
67      uint32_t *copy = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
68      int y;
69
70      if (!copy)
71          return NULL;
72      for (y = 0; y < h; y++)
73          memcpy(copy + (size_t)y * w, s->pixels + (size_t)(area->top + y) * s->width + area->left,
74              (size_t)w * sizeof(uint32_t));
75      return copy;
76  }
```

```text
78  // Function: History_PasteRect
79  // The opposite: puts a packed array back into the surface.
80  static void
81  History_PasteRect(Surface *s, const RECT *area, const uint32_t *pixels)
82  {
83      int w = area->right - area->left;
84      int h = area->bottom - area->top;
85      int y;
86
87      for (y = 0; y < h; y++)
88          memcpy(s->pixels + (size_t)(area->top + y) * s->width + area->left,
89              pixels + (size_t)y * w, (size_t)w * sizeof(uint32_t));
90  }
```

The surface is one long array with `width` pixels per row, so the pixels of a rectangle aren't in one piece. We copy it row by row with `memcpy`. The `copy` array has no gaps. Its rows are exactly `w` pixels wide. That is what "tightly packed" means.

### History_Push

This is called when a change is finished.

```text
 92  BOOL
 93  History_Push(History *h, const Surface *base, const Surface *current, const RECT *area)
 94  {
 95      HistoryItem item;
 96      RECT clip = { 0, 0, current->width, current->height };
 97
 98      ZeroMemory(&item, sizeof(item));
 99      if (!IntersectRect(&item.area, area, &clip))
100          return TRUE; // Nothing changed inside the surface, nothing to remember
101
102      // A new change means the redo list is no longer valid
103      History_DropFrom(h, h->position);
104
105      item.before = History_CopyRect(base, &item.area);
106      item.after = History_CopyRect(current, &item.area);
107      if (!item.before || !item.after)
108      {
109          HistoryItem_Free(&item);
110          History_Clear(h);
111          return FALSE;
112      }
113      item.bytes = 2 * (size_t)(item.area.right - item.area.left) * (item.area.bottom - item.area.top) * sizeof(uint32_t);
```

First we trim the area so it lies inside the surface. If nothing is left, then there is nothing to remember and we are done. Then the line that makes redo behave properly [line 103]. If you have undone three steps and then draw something new, the three undone steps can never come back. `History_DropFrom` frees everything from the current position on. Then we copy the pixels from `base` for the "before" and from `current` for the "after". If either copy fails, we clear the whole history and give up. It is better to lose undo than to undo to the wrong place.

```text
115      if (h->count == h->capacity)
116      {
117          int newCap = h->capacity ? h->capacity * 2 : 32;
118          HistoryItem *bigger = (HistoryItem *)realloc(h->items, (size_t)newCap * sizeof(HistoryItem));
119          if (!bigger)
120          {
121              HistoryItem_Free(&item);
122              History_Clear(h);
123              return FALSE;
124          }
125          h->items = bigger;
126          h->capacity = newCap;
127      }
128      h->items[h->count++] = item;
129      h->position = h->count;
130      h->bytes += item.bytes;
```

Now the new item goes on the end. The array starts at room for 32 items and doubles whenever it fills up, just like the seed stack in chapter 11. `position` is set to `count`, which says "everything has been done".

```text
132      // Too much memory? Forget the oldest changes. We always keep the newest one.
133      while (h->bytes > h->limit && h->count > 1)
134      {
135          h->bytes -= h->items[0].bytes;
136          HistoryItem_Free(&h->items[0]);
137          memmove(&h->items[0], &h->items[1], (size_t)(h->count - 1) * sizeof(HistoryItem));
138          h->count--;
139          h->position--;
140      }
141      return TRUE;
```

Finally, the memory limit. Each item costs two copies of its rectangle. A full 640 by 480 picture is about 1.2 MB, so a change that touches all of it costs about 2.4 MB. If the total goes over the limit, we throw away the oldest item and slide the rest down with `memmove`, until we are back under. The `count > 1` means we always keep the newest change, even if that one change on its own is over the limit.

### Undo and redo

After all that, undo and redo are almost nothing.

```text
156  BOOL
157  History_Undo(History *h, Surface *s, RECT *area)
158  {
159      HistoryItem *item;
160
161      if (!History_CanUndo(h))
162          return FALSE;
163      item = &h->items[--h->position];
164      History_PasteRect(s, &item->area, item->before);
165      *area = item->area;
166      return TRUE;
167  }
```

```text
169  BOOL
170  History_Redo(History *h, Surface *s, RECT *area)
171  {
172      HistoryItem *item;
173
174      if (!History_CanRedo(h))
175          return FALSE;
176      item = &h->items[h->position++];
177      History_PasteRect(s, &item->area, item->after);
178      *area = item->area;
179      return TRUE;
180  }
```

Each steps `position` one way or the other, pastes the right set of pixels and hands the changed rectangle back through `area`, so that the caller knows which part of the screen to repaint. Both say `FALSE` if there is nothing to do. `History_CanUndo` and `History_CanRedo` just compare `position` with `0` and `count`.

### Wiring it up

The history belongs to the document, because each picture has its own list of changes.

```text
24      // Undo may use up to 256 MB before it starts forgetting the oldest changes
25      doc->history = History_Create(256u * 1024 * 1024);
26      if (!doc->history)
27      {
28          Surface_Destroy(doc->surface);
29          free(doc);
30          return NULL;
31      }
```

`Doc_Create` makes a history with a limit of 256 MB, and `Doc_Destroy` frees it. Create a new image with **File, New** and it gets a fresh, empty history, because it's a new `Document`.

The other half is recording changes. That happens when a tool lets go of the mouse.

```text
214  Tool_MouseUp(ToolState *st, Document *doc, ToolSettings *ts, int x, int y, UINT keys)
215  {
216      Tool_MouseMove(st, doc, ts, x, y, keys);
217      if (!st->edit)
218          return;
219
220      doc->modified = TRUE;
221      // The pixels are already where they should be. We keep a before and an
222      // after copy of the area that changed, so the user can undo it.
223      History_Push(doc->history, st->edit->base, doc->surface, &st->edit->bounds);
224      Edit_End(st->edit);
225      st->edit = NULL;
226  }
```

One new line [line 223] does it. `Tool_MouseUp` hands the edit's `base`, the surface and `bounds` to `History_Push`. That is why `Edit` has been remembering these things all along. The fill, the shapes, the brush and the eraser all end up here, so they can all be undone, and we didn't have to touch any of them. (The picker doesn't use an edit, as it doesn't change any pixels, so there is nothing to push.)

### Telling the main window

The menu and toolbar belong to the main window, but the strokes happen in the canvas. When a stroke ends, the main window needs to know so it can enable the Undo button. We have used this pattern since chapter 7. The canvas sends a message to its parent.

```text
583          {
584              Tool_MouseUp(&cv->tool, cv->doc, cv->settings, ix, iy, keys);
585              cv->drawing = FALSE;
586              ReleaseCapture();
587              SendMessageW(GetParent(cv->hwnd), WMU_DOC_CHANGED, 0, 0);
588          }
```

`WMU_DOC_CHANGED` is the new message, defined in `canvas.h` as `WM_APP + 4`. The main window answers it by refreshing the Undo and Redo buttons.

### Updating the buttons

```text
388  // Function: MainWindow_UpdateUndoUI
389  // Greys out Undo and Redo, in the menu and on the toolbar, when there is nothing to do.
390  static void
391  MainWindow_UpdateUndoUI(MainWindow *mw)
392  {
393      BOOL canUndo = mw->doc && History_CanUndo(mw->doc->history);
394      BOOL canRedo = mw->doc && History_CanRedo(mw->doc->history);
395      HMENU menu = GetMenu(mw->hwnd);
396
397      EnableMenuItem(menu, IDM_EDIT_UNDO, MF_BYCOMMAND | (canUndo ? MF_ENABLED : MF_GRAYED));
398      EnableMenuItem(menu, IDM_EDIT_REDO, MF_BYCOMMAND | (canRedo ? MF_ENABLED : MF_GRAYED));
399      SendMessageW(mw->hToolbar, TB_ENABLEBUTTON, IDM_EDIT_UNDO, MAKELPARAM(canUndo, 0));
400      SendMessageW(mw->hToolbar, TB_ENABLEBUTTON, IDM_EDIT_REDO, MAKELPARAM(canRedo, 0));
401  }
```

Two places need to change, the menu and the toolbar.

```c
DWORD EnableMenuItem(HMENU hMenu,           // The menu
                     UINT uIDEnableItem,    // Which item. Here an id, because of MF_BYCOMMAND
                     UINT uEnable);         // MF_BYCOMMAND to say "that was an id", plus MF_ENABLED or MF_GRAYED
```

```c
// TB_ENABLEBUTTON, a message we send to the toolbar
// wParam: the command id of the button
// lParam: MAKELPARAM(enable, 0). Non-zero enables the button, zero greys it
```

The toolbar buttons use the same ids as the menu items, `IDM_EDIT_UNDO` and `IDM_EDIT_REDO`, so one id does for both. That function is also called whenever a new document is set, so a brand new image starts with both buttons grey.

### MainWindow_Undo

```text
362  // Function: MainWindow_Undo
363  // Steps back (or forward, if redo is TRUE) through the history.
364  static void
365  MainWindow_Undo(MainWindow *mw, BOOL redo)
366  {
367      RECT area;
368      BOOL done;
369
370      // The accelerator keys still work while the mouse is down. Not then, thanks.
371      // (We ask the canvas, not GetCapture: a slider being dragged has the mouse captured too.)
372      if (!mw->doc || Canvas_IsBusy(mw->hCanvas))
373          return;
374
375      if (redo)
376          done = History_Redo(mw->doc->history, mw->doc->surface, &area);
377      else
378          done = History_Undo(mw->doc->history, mw->doc->surface, &area);
379
380      if (done)
381      {
382          mw->doc->modified = TRUE;
383          Canvas_InvalidateImageRect(mw->hCanvas, &area);
384      }
385      MainWindow_UpdateUndoUI(mw);
386  }
```

The menu, the toolbar and the Ctrl keys all end up in `MainWindow_OnCommand`, which now calls this for `IDM_EDIT_UNDO` and `IDM_EDIT_REDO`. It asks the history to step, marks the document as modified, and repaints the changed rectangle with `Canvas_InvalidateImageRect`. That used to be a private function. It is now public, with a new `HWND` first parameter, so the main window can use it. Inside `canvas.c`, the private version is called `Canvas_InvalidateRect`, to avoid a clash.

### Don't undo in the middle of a stroke

Look at the guard at the top of `MainWindow_Undo`.

Accelerator keys still work while you hold the mouse button. Press Ctrl+Z halfway through a rectangle and the surface would be changed under the tool's feet, while the `Edit` is still holding a copy of what it thought the picture looked like. We don't want that, so the undo is refused when the user is busy.

How do you know if the mouse is busy? An obvious answer is `GetCapture`, which returns the window that has the mouse captured. During a stroke that is our canvas, so "is anyone capturing the mouse?" looks like a fair test. The trouble is that other controls capture the mouse too. Drag the alpha slider in the palette and the trackbar has the capture. A combo box does the same while its list is open. None of those is a stroke, and none of them should make Undo behave differently.

So we ask the canvas itself.

```text
483  BOOL
484  Canvas_IsBusy(HWND canvas)
485  {
486      Canvas *cv = (Canvas *)GetWindowLongPtrW(canvas, GWLP_USERDATA);
487      return cv->drawing || cv->panning;
488  }
```

The canvas knows whether a stroke or a pan is in progress, because it keeps the flags `drawing` and `panning`. Asking the one that owns the information is always more reliable than guessing from outside.

### What it can't do

This is a simple design, and it has limits.

- The history stores the whole bounding rectangle of each change, including pixels that didn't change. A long diagonal line saves a big rectangle, most of it untouched. A shape you dragged out to a huge size and back down again saves the huge rectangle. It is wasteful, but it is easy to get right.
- When the history goes over 256 MB, the oldest changes are forgotten without a word.
- If we run out of memory while saving a change, `History_Push` clears the history and returns `FALSE`. `Tool_MouseUp` doesn't look at the return value. The picture is fine, but Undo goes grey.
- Only pixel changes are tracked. As we add more kinds of change later, they will have to find a way to be recorded too.

## Adding functionality

To see the memory limit in action, make it tiny. In `doc.c`, change the line that creates the history [line 25] to

```c
doc->history = History_Create(1024u * 1024);   // 1 MB
```

Build, then draw ten big shapes and press Ctrl+Z over and over. You will only get a few steps back before Undo greys out. The rest were thrown away to stay under the limit. Change it back to 256 MB when you have seen enough.

## Exercise

Show how many steps can be undone and redone in the status bar. Something like "Undo: 3, Redo: 1".

*Hint: In `MainWindow_UpdateUndoUI`, `mw->doc->history->position` is the number of steps that can be undone, and `count - position` is the number that can be redone. Build the text with `StringCchPrintfW` and send it to the status bar with `SB_SETTEXTW`, as `MainWindow_OnZoomChanged` does.*

## That's it

Congratulations! You can now make mistakes without fear, which is the best feature a paint program can have. Next we put the picture on disk, so that it survives closing the program.

[Chapter 14: Open and save](../14-open-and-save-bmp/README.md)
