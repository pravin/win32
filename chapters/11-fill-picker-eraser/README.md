# Chapter 11 - Fill, picker and eraser

[< Chapter 10: Shapes](../10-shapes/README.md)

In this chapter you will

- fill an area with the paint bucket, using a "scanline" flood fill,
- choose how fussy the bucket is with the **Fill Tolerance** menu,
- pick up a colour from the picture with the colour picker,
- rub things out with the eraser,
- decide the mouse pointer yourself with `WM_SETCURSOR`.

## Before we begin

This chapter has a new pair of files, `src/fill.c` and `src/fill.h`, which hold the paint bucket. `CMakeLists.txt` lists `fill.c`, so a normal build picks it up.

```text
cmake -S . -B build && cmake --build build
```

The rest is small changes. `tools.c` and `tools.h` get the three new tools, `canvas.c` gets the pointer code, `mainwindow.c` gets the tolerance menu, and `Edit_FillSpan` in `edit.c` is no longer private. If you need to remember how to run the program, [chapter 1](../01-hello-win32/README.md) covers it.

Draw a few shapes with the tools from the last chapter, pick the bucket (**Fill**) and click inside one.

![The paint bucket filling a shape](images/screenshot.png)

Left click fills with the primary colour and right click fills with the secondary. The picker copies a colour out of the picture, and the eraser paints with the secondary colour. Let's see how each one works.

## Breaking it up

### The paint bucket

The idea is simple. You click on a pixel. The bucket spreads out from there to every neighbouring pixel that is the same colour, and the neighbours of those, and so on, until it is stopped by pixels of a different colour. Then it paints everything it reached.

Think of pouring a bucket of water into one room of a house. The water spreads over the whole floor and stops at the walls. If the doors are open it flows into the next room as well. A paint bucket is exactly that, with the floor made of pixels.

The obvious way to write it is a function that colours a pixel and then calls itself on the four neighbours. It works on small pictures, then crashes on a big one, because a big empty area makes the function call itself hundreds of thousands of times, and the stack runs out. We need something that doesn't go deeper and deeper.

The answer is a **scanline fill**. Instead of one pixel at a time, we fill a whole horizontal run, then look at the rows above and below that run to find where to go next. That does a lot more work per step and needs far fewer steps. The list of "places still to look at" lives on the heap, in a stack that grows as needed.

```text
11  typedef struct Seed
12  {
13      int x, y;
14  } Seed;
15
16  // Struct: SeedStack
17  // A list of places still to look at, that grows as needed.
18  typedef struct SeedStack
19  {
20      Seed *items;
21      size_t count;
22      size_t capacity;
23  } SeedStack;
24
25  static BOOL
26  Stack_Push(SeedStack *st, int x, int y)
27  {
28      if (st->count == st->capacity)
29      {
30          size_t newCap = st->capacity ? st->capacity * 2 : 256;
31          Seed *bigger = (Seed *)realloc(st->items, newCap * sizeof(Seed));
32          if (!bigger)
33              return FALSE;
34          st->items = bigger;
35          st->capacity = newCap;
36      }
37      st->items[st->count].x = x;
38      st->items[st->count].y = y;
39      st->count++;
40      return TRUE;
41  }
```

A `Seed` is a pixel we have promised to look at. `Stack_Push` adds one to the end, doubling the memory with `realloc` whenever it is full. Nothing here is Windows specific. It is plain C.

### Fill_Close

How do we decide whether two pixels are the same colour?

```text
43  // Function: Fill_Close
44  // Is pixel b close enough to pixel a? Every channel must be within the tolerance.
45  static BOOL
46  Fill_Close(uint32_t a, uint32_t b, int tolerance)
47  {
48      return abs((int)PIX_A(a) - (int)PIX_A(b)) <= tolerance
49          && abs((int)PIX_R(a) - (int)PIX_R(b)) <= tolerance
50          && abs((int)PIX_G(a) - (int)PIX_G(b)) <= tolerance
51          && abs((int)PIX_B(a) - (int)PIX_B(b)) <= tolerance;
52  }
```

Each of the four channels (alpha, red, green, blue) has to be within `tolerance` of the same channel in the start pixel. With a tolerance of 0 the colours must match exactly. This works on the pixel values as they are stored, which are premultiplied, so a see-through pixel is compared by what is in memory, not by the colour you would see.

### Fill_Flood

```text
54  void
55  Fill_Flood(Edit *e, int x, int y, int tolerance)
56  {
57      Surface *s = e->base;       // Look at the original pixels, not the ones we are painting
58      SeedStack stack = { NULL, 0, 0 };
59      uint32_t seedColor;
60      int w = s->width, h = s->height;
61
62      if (x < 0 || y < 0 || x >= w || y >= h)
63          return;
64
65      // We use the fill mask to remember which pixels we have already filled.
66      // Filling an empty span makes sure the mask exists.
67      Edit_FillSpan(e, y, x, x);
68      seedColor = s->pixels[(size_t)y * w + x];
```

Notice that `s` is `e->base`, the picture as it was before the click. The edit is painting on `e->target` as it goes, and if we looked at those pixels we would be chasing our own paint. We use the fill mask for bookkeeping too. A pixel is "done" once it has a value in `fillMask`, which we met in chapter 10. Calling `Edit_FillSpan` on just the starting pixel [line 67] makes sure that mask exists before we look at it.

```text
72      Stack_Push(&stack, x, y);
73      while (stack.count > 0)
74      {
75          Seed seed = stack.items[--stack.count];
76          int left = seed.x, right = seed.x;
77          int dy;
78
79          // Another run may have already filled this pixel
80          if (e->fillMask[(size_t)seed.y * w + seed.x] && !(seed.x == x && seed.y == y))
81              continue;
82          if (!Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + seed.x], tolerance))
83              continue;
84
85          // Stretch the run as far as we can both ways
86          while (left > 0 && !e->fillMask[(size_t)seed.y * w + left - 1]
87              && Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + left - 1], tolerance))
88              left--;
89          while (right < w - 1 && !e->fillMask[(size_t)seed.y * w + right + 1]
90              && Fill_Close(seedColor, s->pixels[(size_t)seed.y * w + right + 1], tolerance))
91              right++;
92
93          Edit_FillSpan(e, seed.y, left, right);
```

Here is the loop. We pop a seed off the stack. If something has filled it already, or it isn't close to the start colour, we skip it. (The odd looking `seed.x == x && seed.y == y` [line 80] is there because we marked the very first pixel in the mask a moment ago, and we still want to process it.) Then we stretch left and right from the seed [lines 86 to 91] for as long as the pixels are close enough and not yet done, and fill that whole run with one call to `Edit_FillSpan` [line 93].

```text
 95          // Look at the rows above and below. Each separate stretch of matching
 96          // pixels gets one seed.
 97          for (dy = -1; dy <= 1; dy += 2)
 98          {
 99              int ny = seed.y + dy;
100              int px;
101              BOOL inRun = FALSE;
102
103              if (ny < 0 || ny >= h)
104                  continue;
105              for (px = left; px <= right; px++)
106              {
107                  size_t i = (size_t)ny * w + px;
108                  BOOL ok = !e->fillMask[i] && Fill_Close(seedColor, s->pixels[i], tolerance);
109                  if (ok && !inRun)
110                      Stack_Push(&stack, px, ny);
111                  inRun = ok;
112              }
113          }
114      }
115      free(stack.items);
116  }
```

Last, we look at the row above and the row below, along the stretch we just filled. Each separate group of matching pixels gets exactly one seed. `inRun` remembers whether the previous pixel was a match, so we only push at the start of a group. Then we go round the loop again until the stack is empty.

Don't worry if this takes a couple of reads. Draw a small blob on paper and follow it with a pencil. That is how I worked it out.

### Tolerance, and the halo problem

Why is there a tolerance at all? Try this. Draw a circle with the **Ellipse** tool, then set **Options, Fill Tolerance** to **0%** and fill the inside. You should see a thin ring of the old colour left between the new fill and the outline.

The outline is anti-aliased. The brush softens its edge by a pixel so circles don't look jagged, which means the pixels along the edge are a blend of the outline colour and the background. Those pixels are neither white nor black. With an exact match the bucket stops at the first one it sees, and leaves a halo behind. A bit of tolerance lets it spread into the blended pixels, and the fill reaches the outline.

So the default is 128, which the menu calls **50%**. Here is how the menu items map onto the number the bucket uses.

```text
248      if (id >= IDM_TOL_FIRST && id <= IDM_TOL_LAST)
249      {
250          static const int tolerances[] = { 0, 25, 64, 128 }; // 0%, 10%, 25%, 50% of 255, roughly
251          mw->settings.tolerance = tolerances[id - IDM_TOL_FIRST];
252          MainWindow_CheckRadio(mw, IDM_TOL_FIRST, IDM_TOL_LAST, id);
253          return;
254      }
```

The values are 0, 25, 64 and 128 out of 255, which is about 0%, 10%, 25% and 50%. The comment says "roughly" and it means it. The percentages are only labels.

The catch is that a tolerance that high will happily flood across two colours that are fairly close. Lower it when the bucket leaks into places you didn't want.

The fill itself is hard edged. Every pixel it reaches gets the full colour, so the new boundary has no anti-aliasing of its own. That is a known limitation, and a reason to keep the tolerance high enough to swallow the soft edge.

### Starting the bucket

The tools get their new cases in `Tool_MouseDown`.

```text
160      case TOOL_FILL:
161          st->edit = Edit_Begin(doc->surface);
162          if (!st->edit)
163              return;
164          // The fill colour is the colour of the button you pressed
165          Edit_SetFillColor(st->edit, color);
166          Fill_Flood(st->edit, x, y, ts->tolerance);
167          break;
168      case TOOL_PICKER:
169          Tool_Pick(st, doc, ts, x, y);
170          break;
```

A fill is a one-shot tool. There is no dragging. We start an `Edit`, tell it the fill colour (the colour of the button you pressed, which is the `color` we worked out at the top of the function), and call `Fill_Flood`. When the button comes up the edit is finished like any other.

### The colour picker

The picker doesn't change any pixels, so it doesn't need an `Edit` at all.

```text
101  // Function: Tool_Pick
102  // The eyedropper. Copies the colour under the mouse into the primary
103  // (or secondary) colour.
104  static void
105  Tool_Pick(ToolState *st, Document *doc, ToolSettings *ts, int x, int y)
106  {
107      uint32_t pixel;
108
109      if (x < 0 || y < 0 || x >= doc->width || y >= doc->height)
110          return;
111
112      // Pixels are premultiplied, colours in the settings are not
113      pixel = Pixel_Unpremultiply(Surface_GetPixel(doc->surface, x, y));
114      if (st->useSecond)
115          ts->secondary = pixel;
116      else
117          ts->primary = pixel;
118      st->colorsChanged = TRUE;
119  }
```

It checks that the mouse is inside the picture, reads the pixel with `Surface_GetPixel`, and turns it back into a normal colour with `Pixel_Unpremultiply`. The surface holds premultiplied pixels and the colours in `ToolSettings` don't, so we have to convert, as we saw in chapter 7. The result goes into the primary or the secondary colour, depending on the button. The alpha comes along too, so picking a see-through pixel gives you a see-through colour.

`Tool_MouseMove` has a few new lines at the top. If the picker is the current tool, it picks again, so dragging the mouse around the picture slides the colour about as you go. That is a nice way to hunt for the shade you want.

There is one new problem. The picker changes `ToolSettings`, and the colour swatches in the palette have no idea. The tool can't draw on the palette, it doesn't know the palette exists. So it sets a new flag, `colorsChanged`, in `ToolState`.

```text
237      Canvas_RepaintDirty(cv);
238      if (cv->tool.colorsChanged)
239      {
240          cv->tool.colorsChanged = FALSE;
241          SendMessageW(GetParent(cv->hwnd), WMU_COLORS_CHANGED, 0, 0);
242      }
243  }
```

The canvas notices the flag, clears it, and sends a message to its parent, the main window. This is the same pattern as `WMU_CANVAS_POS` from chapter 7. The new message is `WMU_COLORS_CHANGED`, and the main window handles it by calling `Palette_Refresh`, which redraws the swatches.

To let the picker write to the settings, `ToolSettings *` replaces `const ToolSettings *` through `Tool_MouseDown`, `Tool_MouseMove`, `Tool_MouseUp` and `Canvas_SetSettings`. If you see compiler errors in your own changes, that is the first place to look.

### The eraser

```text
144      case TOOL_ERASER:
145          // Like classic Paint, the eraser paints with the secondary colour, fully solid
146          st->edit = Edit_Begin(doc->surface);
147          if (!st->edit)
148              return;
149          Edit_SetBrush(st->edit, ts->secondary | 0xFF000000, ts->brushSize, FALSE);
150          Edit_Stamp(st->edit, x, y);
151          break;
```

The eraser is a brush with two differences. It always paints with the secondary colour, and it forces the alpha to `0xFF` [line 149], so the paint is solid and hard edged whatever the soft brush option says. That is how the old Paint worked, and the secondary colour starts as white, so out of the box it looks like a real eraser.

Here is what it isn't. It doesn't make pixels see-through. It paints them, so if you change the secondary colour the eraser changes with it. Erasing to transparency would need a picture that can be see-through, and ours is opaque white to begin with.

In `Tool_MouseMove`, `TOOL_ERASER` shares the pencil's case, so fast strokes are joined up with lines.

### The mouse pointer

Until now the canvas window class had a cross hair as its cursor, and it was shown everywhere, even over the grey workspace. The cross hair is good over the picture and odd outside it. So we take charge. In `Canvas_RegisterClass` the class cursor is now `NULL`, with a comment pointing here. Windows sends `WM_SETCURSOR` to the window under the mouse whenever it needs to decide which pointer to show, and the canvas answers.

```text
169  // Function: Canvas_OnSetCursor
170  // Windows asks which mouse pointer to show. Over the image we want the
171  // cross hair, anywhere else the normal arrow.
172  //
173  // Returns:
174  //   TRUE if we set the cursor, FALSE to let Windows do it.
175  static BOOL
176  Canvas_OnSetCursor(const Canvas *cv, int hitTest)
177  {
178      POINT pt, origin;
179      BOOL overImage;
180
181      if (hitTest != HTCLIENT || !cv->doc)
182          return FALSE;
183
184      GetCursorPos(&pt);                  // Screen co-ordinates
185      ScreenToClient(cv->hwnd, &pt);      // Now window co-ordinates
186      origin = Canvas_ImageOrigin(cv);
187      overImage = pt.x >= origin.x && pt.y >= origin.y
188          && pt.x < origin.x + cv->doc->width && pt.y < origin.y + cv->doc->height;
189
190      // While drawing, keep the cross hair even if we stray off the edge
191      SetCursor(LoadCursorW(NULL, (overImage || cv->drawing) ? IDC_CROSS : IDC_ARROW));
192      return TRUE;
193  }
```

`hitTest` says which part of the window the mouse is over. We only care about `HTCLIENT`, the inside, so we let Windows deal with the borders. We ask where the mouse is with `GetCursorPos`, which gives screen co-ordinates, and `ScreenToClient` turns them into window co-ordinates. If that is over the image, or if we are in the middle of a stroke, it's a cross hair. Otherwise it's the arrow. Returning `TRUE` from the window procedure [line 113] tells Windows we have set the cursor.

Why not just call `SetCursor` when the mouse moves? Because Windows puts the class cursor back whenever the mouse moves, so a pointer set from somewhere else would be undone almost at once. `WM_SETCURSOR` is the proper place.

## Adding functionality

Let's add a 75% tolerance to the menu. It takes three small edits.

1. In `res/drawlite.rc`, add a line after the `50%` item in the **Fill Tolerance** popup.

```text
MENUITEM "&75%",                IDM_TOL_75
```

2. In `src/resource.h`, add the new id and move `IDM_TOL_LAST` down so it covers it.

```c
#define IDM_TOL_75          634
#define IDM_TOL_LAST        634
```

3. In `src/mainwindow.c`, add a value to the end of the `tolerances` array. 75% of 255 is 191.

```c
static const int tolerances[] = { 0, 25, 64, 128, 191 };
```

The menu handler already looks at everything from `IDM_TOL_FIRST` to `IDM_TOL_LAST`, so nothing else changes. Try filling the same drawing at 0% and 75% and see what the bucket does at the edges.

## Exercise

Write a "fill all" mode. Instead of filling only the region connected to the pixel you clicked, fill every pixel in the picture that is close to its colour, wherever it is. Hold Ctrl while clicking the bucket to use it.

*Hint: You don't need the stack. Loop over every row, find runs of pixels where `Fill_Close` says yes, and hand each one to `Edit_FillSpan`. Test `keys & MK_CONTROL` in `Tool_MouseDown`.*

## That's it

Congratulations! You have a pencil, brush, shapes, a bucket, a picker and an eraser. That is a real paint program. Next, let's zoom in and look at the pixels up close.

[Chapter 12: Zoom and scroll](../12-zoom-and-scroll/README.md)
