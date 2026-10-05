# Chapter 9 - Colour

[< Chapter 8: Pencil and brush](../08-pencil-and-brush/README.md)

Time to stop painting in black and white. This chapter gives DrawLite two paint colours, a row of quick colours to click on, a swap button, an alpha slider for see-through paint, and the standard Windows colour dialog.

In this chapter you will

- keep a primary and a secondary colour, and swap them,
- pick colours from a grid, or from the Windows colour dialog (`ChooseColorW`),
- learn why Windows and DrawLite store colours in different byte orders (and how to convert),
- use a trackbar control and its `WM_HSCROLL` message to set alpha,
- draw a swatch that shows transparency with a chequerboard behind it.

## Before we begin

Almost all of the work is in `src/palette.c`, which grows from a column of tool buttons into a small control panel with its own state. A few other files change a little.

| File | Change |
|------|--------|
| `src/palette.c`, `palette.h` | Swatches, quick colours, Swap button, alpha slider, colour dialog |
| `src/pixel.h` | Two new helpers, `Pixel_FromColorRef` and `Pixel_ToColorRef` |
| `src/mainwindow.c` | The settings are filled in earlier, and the palette is given a pointer to them |
| `src/resource.h` | Three new control ids |
| `CMakeLists.txt` | Links `comdlg32`, the library that holds the colour dialog |

If you build with the command line from chapter 1, add `comdlg32.lib` or `-lcomdlg32`. With CMake it's already done.

```text
cmake -S . -B build && cmake --build build
```

Run it. Under the tool buttons you'll find two overlapping squares (primary in front, secondary behind), a **Swap** button, sixteen small colour cells and an **Alpha** slider.

![The palette with two swatches, sixteen quick colours and an alpha slider](images/screenshot.png)

Left click a quick colour to set the primary colour. Right click one to set the secondary. Click on a big swatch to open the colour dialog. Draw with the left mouse button and you get the primary colour, with the right button the secondary. Drag the alpha slider down, draw, and you'll see the paint go see-through.

## Breaking it up

### Two kinds of colour

Windows has its own idea of a colour. It's a `COLORREF`, a 32 bit number with red in the *low* byte, `0x00BBGGRR`. It's what `RGB(r, g, b)`, `CreateSolidBrush` and the colour dialog all use. It has no alpha.

DrawLite's colours are `0xAARRGGBB`. Red is in the third byte, and there is an alpha. So whenever a colour crosses from one world to the other we need to swap the bytes around.

```text
65  // Function: Pixel_FromColorRef
66  // Windows describes colours as a COLORREF, 0x00BBGGRR (red is the LOW byte).
67  // This turns one into our 0xAARRGGBB form, with the alpha you give it.
68  static inline uint32_t
69  Pixel_FromColorRef(uint32_t colorRef, uint32_t alpha)
70  {
71      return PIX_MAKE(alpha, colorRef & 0xFF, (colorRef >> 8) & 0xFF, (colorRef >> 16) & 0xFF);
72  }
```

```text
74  // Function: Pixel_ToColorRef
75  // The other way round. The alpha is dropped.
76  static inline uint32_t
77  Pixel_ToColorRef(uint32_t c)
78  {
79      return PIX_R(c) | (PIX_G(c) << 8) | (PIX_B(c) << 16);
80  }
```

`Pixel_FromColorRef` takes a `COLORREF` and the alpha you want and gives you back a DrawLite colour. `Pixel_ToColorRef` goes the other way and throws the alpha away. Both are about getting the bytes in the right places. Notice that these are *straight* colours. They aren't premultiplied. That's the form that `ToolSettings` holds, as we saw in the last chapter, and the `Edit` premultiplies them later.

### The palette gets state

Until now the palette needed no memory of its own. Now it has two big things to look after: a pointer to the shared settings, and a few handles.

```text
31  // Struct: Palette
32  typedef struct Palette
33  {
34      HWND hwnd;
35      ToolSettings *settings;
36      HWND hAlpha;                // The alpha trackbar
37      HWND hAlphaLabel;           // "Alpha: 255"
38      HWND hSwap;                 // The Swap button
39      COLORREF customColors[16];  // Colours the user has mixed in the colour dialog
40  } Palette;
41
42  // Struct: PaletteLayout
43  // Where everything sits, in window co-ordinates.
44  typedef struct PaletteLayout
45  {
46      RECT primary;
47      RECT secondary;
48      RECT swap;
49      RECT quick;         // The whole grid of quick colours
50      int cell;           // Size of one quick colour cell
51      RECT alphaLabel;
52      RECT alphaBar;
53  } PaletteLayout;
```

We keep that in a `Palette` struct, in `GWLP_USERDATA`, exactly the way we did for the canvas in chapter 7. `WM_NCCREATE` stores it, `WM_NCDESTROY` frees it.

```text
355      case WM_NCCREATE:
356          p = (Palette *)((CREATESTRUCTW *)lParam)->lpCreateParams;
357          p->hwnd = hwnd;
358          SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)p);
359          break;
360      case WM_CREATE:
361          return Palette_OnCreate(p) ? 0 : -1;
362      case WM_NCDESTROY:
363          free(p);
364          SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
365          break;
```

There's one new twist. Windows sends `WM_CREATE` after `WM_NCCREATE`. It's the right moment to make the child controls, because the window exists by then. If we return -1 from `WM_CREATE`, window creation fails.

```text
106  // Function: Palette_OnCreate
107  // Makes the buttons, the slider and the label.
108  static BOOL
109  Palette_OnCreate(Palette *p)
110  {
111      HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(p->hwnd, GWLP_HINSTANCE);
112      int i;
113
114      // One push-like radio button per tool. Radio buttons give us
115      // "only one pressed at a time" for free.
116      for (i = 0; i < TOOL_COUNT; i++)
117      {
118          DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE;
119          if (i == 0)
120              style |= WS_GROUP | WS_TABSTOP;
121          CreateWindowExW(0, L"BUTTON", Tool_Name((ToolId)i), style, 0, 0, 0, 0,
122              p->hwnd, (HMENU)(INT_PTR)(IDM_TOOL_FIRST + i), hInstance, NULL);
123      }
124
125      p->hSwap = CreateWindowExW(0, L"BUTTON", L"Swap", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
126          0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_SWAP, hInstance, NULL);
127      p->hAlphaLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
128          0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_ALPHA_LABEL, hInstance, NULL);
129      p->hAlpha = CreateWindowExW(0, TRACKBAR_CLASSW, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
130          0, 0, 0, 0, p->hwnd, (HMENU)(INT_PTR)IDC_ALPHA, hInstance, NULL);
131      if (!p->hSwap || !p->hAlphaLabel || !p->hAlpha)
132          return FALSE;
133
134      SendMessageW(p->hAlpha, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
135      SendMessageW(p->hAlpha, TBM_SETPOS, TRUE, PIX_A(p->settings->primary));
136      Palette_UpdateAlphaLabel(p);
137      return TRUE;
138  }
```

The tool buttons are as before. New are the Swap button, a static text label for "Alpha: 255", and the trackbar, which is the slider.

Notice `p->settings->primary` is read here to set the slider position. That means `settings` has to be filled in *before* the palette is created, which is why the starting values in `mainwindow.c` moved a few lines up and `Palette_Create` takes a `ToolSettings *`. The palette doesn't own the settings. It writes its colour changes straight into them, and the canvas reads the same struct when you draw.

### One layout, three users

The palette needs to know where each thing is for three jobs. To move the child controls, to paint the swatches, and to work out which swatch was clicked. If each of those did its own sums, sooner or later they'd disagree and you'd click on a colour that isn't where it looks.

So there is one function that works it all out, and the three jobs share it.

```text
166  // Function: Palette_GetLayout
167  // Works out where everything goes. Used for laying out the child windows,
168  // for painting, and for working out what was clicked, so they always agree.
169  static void
170  Palette_GetLayout(const Palette *p, PaletteLayout *lay)
171  {
172      UINT dpi = GetDpiForWindow(p->hwnd);
173      int margin = Ui_Scale(dpi, 6);
174      int toolRows = (TOOL_COUNT + 1) / 2;
175      int toolsBottom = margin + toolRows * (Ui_Scale(dpi, 28) + margin / 2);
176      int swatch = Ui_Scale(dpi, 36);
177      int overlap = Ui_Scale(dpi, 22);
178      int top = toolsBottom + Ui_Scale(dpi, 12);
179      RECT client;
180
181      GetClientRect(p->hwnd, &client);
182
183      // The secondary swatch sits behind and to the right of the primary one
184      SetRect(&lay->secondary, margin + overlap, top + overlap, margin + overlap + swatch, top + overlap + swatch);
185      SetRect(&lay->primary, margin, top, margin + swatch, top + swatch);
186      SetRect(&lay->swap, margin + overlap + swatch + margin, top, client.right - margin, top + Ui_Scale(dpi, 24));
187
188      lay->cell = (client.right - 2 * margin) / QUICK_COLS;
189      SetRect(&lay->quick, margin, lay->secondary.bottom + Ui_Scale(dpi, 12),
190          margin + lay->cell * QUICK_COLS, lay->secondary.bottom + Ui_Scale(dpi, 12) + lay->cell * QUICK_ROWS);
191
192      SetRect(&lay->alphaLabel, margin, lay->quick.bottom + Ui_Scale(dpi, 10),
193          client.right - margin, lay->quick.bottom + Ui_Scale(dpi, 28));
194      SetRect(&lay->alphaBar, margin, lay->alphaLabel.bottom,
195          client.right - margin, lay->alphaLabel.bottom + Ui_Scale(dpi, 28));
196  }
```

It fills in a `PaletteLayout` full of rectangles. The sizes are scaled for the screen's DPI with `Ui_Scale`, which we met in chapter 5, so the palette looks the same on a high resolution laptop screen as on an old monitor. The secondary swatch is positioned down and to the right of the primary one [line 184], and drawn first so that the primary overlaps it.

### Drawing a swatch

A colour with alpha 128 on a plain background doesn't look like anything useful. You can't tell it from a solid colour that happens to be lighter. Paint programs solve this with a chequerboard behind the colour, so you can see through it.

```text
229  // Function: Palette_DrawSwatch
230  // Fills a rectangle with a colour, including its see-through-ness.
231  // Behind a see-through colour we draw a little chequerboard, then
232  // work out for each square what the colour looks like on top of it.
233  static void
234  Palette_DrawSwatch(HDC hdc, const RECT *r, uint32_t color)
235  {
236      const int cell = 6;
237      uint32_t a = PIX_A(color);
238      int x, y;
239
240      for (y = r->top; y < r->bottom; y += cell)
241      {
242          for (x = r->left; x < r->right; x += cell)
243          {
244              // Light and dark squares alternate
245              uint32_t back = (((x - r->left) / cell + (y - r->top) / cell) & 1) ? 0xB0 : 0xFF;
246              uint32_t rr = Pixel_Mul255(PIX_R(color), a) + Pixel_Mul255(back, 255 - a);
247              uint32_t gg = Pixel_Mul255(PIX_G(color), a) + Pixel_Mul255(back, 255 - a);
248              uint32_t bb = Pixel_Mul255(PIX_B(color), a) + Pixel_Mul255(back, 255 - a);
249              RECT sq = { x, y, min(x + cell, r->right), min(y + cell, r->bottom) };
250              HBRUSH brush = CreateSolidBrush(RGB(rr, gg, bb));
251              FillRect(hdc, &sq, brush);
252              DeleteObject(brush);
253          }
254      }
255      FrameRect(hdc, r, (HBRUSH)GetStockObject(DKGRAY_BRUSH));
256  }
```

The swatch is cut into squares 6 pixels wide. For each one, we pick a background (white or light grey, alternating) [line 245] and then work out what the colour looks like on top of it. The sum is `colour * alpha + background * (1 - alpha)`, once per channel [lines 246 to 248].

That is the straight alpha formula from chapter 7, the one with two multiplications per channel. Here we use it on purpose, because the colour in `settings` is straight. We're doing a handful of squares, not millions of pixels, so the extra multiplication costs nothing. Each result goes into a GDI brush and `FillRect`. At alpha 255 the second term is zero and you get the plain colour. At alpha 0 you see only the chequerboard.

`Palette_OnPaint` calls it for the secondary swatch, then the primary, and then fills the sixteen cells of the grid with plain colours from `g_quickColors`.

```text
22  // The sixteen quick colours, as COLORREFs (0x00BBGGRR)
23  static const COLORREF g_quickColors[QUICK_COLS * QUICK_ROWS] =
24  {
25      RGB(0, 0, 0),       RGB(128, 128, 128), RGB(128, 0, 0),     RGB(128, 128, 0),
26      RGB(0, 128, 0),     RGB(0, 128, 128),   RGB(0, 0, 128),     RGB(128, 0, 128),
27      RGB(255, 255, 255), RGB(192, 192, 192), RGB(255, 0, 0),     RGB(255, 255, 0),
28      RGB(0, 255, 0),     RGB(0, 255, 255),   RGB(0, 0, 255),     RGB(255, 0, 255)
29  };
```

These are `COLORREF`s, so we write them with `RGB`. The grid has two rows of eight.

### Clicking

```text
287  // Function: Palette_OnClick
288  // Works out what was clicked in the swatch and quick colour area.
289  // Left button changes the primary colour, right button the secondary.
290  static void
291  Palette_OnClick(Palette *p, int x, int y, BOOL rightButton)
292  {
293      PaletteLayout lay;
294      POINT pt = { x, y };
295
296      Palette_GetLayout(p, &lay);
297
298      if (PtInRect(&lay.primary, pt))
299      {
300          Palette_PickColor(p, TRUE);
301      }
302      else if (PtInRect(&lay.secondary, pt))
303      {
304          Palette_PickColor(p, FALSE);
305      }
306      else if (PtInRect(&lay.quick, pt))
307      {
308          int col = (x - lay.quick.left) / lay.cell;
309          int row = (y - lay.quick.top) / lay.cell;
310          COLORREF c = g_quickColors[row * QUICK_COLS + col];
311
312          if (rightButton)
313              p->settings->secondary = Pixel_FromColorRef(c, 255);
314          else // Keep whatever alpha the slider is set to
315              p->settings->primary = Pixel_FromColorRef(c, PIX_A(p->settings->primary));
316          InvalidateRect(p->hwnd, NULL, FALSE);
317      }
318  }
```

The click position is tested against each rectangle from the layout with `PtInRect`. A click on a swatch opens the colour dialog for that colour. A click on the grid works out the row and column with a division by the cell size, and looks up the colour.

A left click changes the primary colour and *keeps the alpha from the slider* [line 315]. That way you can set a see-through level, then try different colours without it jumping back to solid. A right click sets the secondary colour, always solid.

### The colour dialog

Windows has a ready made colour chooser. We don't have to build one.

```c
BOOL ChooseColorW(LPCHOOSECOLORW lpcc);     // Pass a filled in CHOOSECOLORW. Returns TRUE if the user pressed OK
```

All the work is in the struct.

```text
320  // Function: Palette_PickColor
321  // Shows the standard Windows colour dialog and stores the result.
322  static void
323  Palette_PickColor(Palette *p, BOOL primary)
324  {
325      CHOOSECOLORW cc;
326      uint32_t *target = primary ? &p->settings->primary : &p->settings->secondary;
327
328      ZeroMemory(&cc, sizeof(cc));
329      cc.lStructSize = sizeof(cc);
330      cc.hwndOwner = p->hwnd;
331      cc.rgbResult = Pixel_ToColorRef(*target);   // Start from the current colour
332      cc.lpCustColors = p->customColors;          // Windows remembers mixed colours here
333      cc.Flags = CC_RGBINIT | CC_FULLOPEN;
334
335      if (ChooseColorW(&cc))
336          *target = Pixel_FromColorRef(cc.rgbResult, PIX_A(*target));
337      InvalidateRect(p->hwnd, NULL, FALSE);
338  }
```

`lStructSize` tells Windows which version of the struct we have. `hwndOwner` makes the dialog belong to the palette. `rgbResult`, with the `CC_RGBINIT` flag, tells the dialog which colour to start from. `CC_FULLOPEN` shows the whole dialog, the one with the colour spectrum, instead of the small cut down version.

`lpCustColors` points to sixteen `COLORREF`s that the dialog uses for "custom colours". When the user mixes a colour and adds it, Windows saves it into our array. We keep that array in the `Palette` struct, so the custom colours survive for as long as the program runs, even when the dialog is closed and opened again. They don't survive restarting the program. That would be a job for the registry or a settings file, and we aren't doing either.

Like `DialogBoxParamW`, `ChooseColorW` doesn't return until the dialog is closed. If it returns `TRUE`, the answer is in `rgbResult`. We convert it back with `Pixel_FromColorRef`, again keeping the old alpha, because the dialog knows nothing about alpha.

### The alpha slider

The slider is a trackbar, one of the common controls that come with comctl32 (chapter 5 gave us the toolbar and status bar from the same library). Its class name is `TRACKBAR_CLASSW`, and we control it with messages.

```c
SendMessageW(hAlpha, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255)); // TRUE = redraw. Low word is min, high word is max
SendMessageW(hAlpha, TBM_SETPOS, TRUE, 128);                   // Move the thumb to 128
int pos = (int)SendMessageW(hAlpha, TBM_GETPOS, 0, 0);         // Where is the thumb now?
```

When the user drags the thumb, the trackbar tells its *parent* with `WM_HSCROLL`. That parent is the palette.

```text
376      case WM_HSCROLL:
377          // The trackbar tells its parent about every movement with WM_HSCROLL
378          if ((HWND)lParam == p->hAlpha)
379          {
380              uint32_t alpha = (uint32_t)SendMessageW(p->hAlpha, TBM_GETPOS, 0, 0);
381              p->settings->primary = (p->settings->primary & 0x00FFFFFF) | (alpha << 24);
382              Palette_UpdateAlphaLabel(p);
383              InvalidateRect(hwnd, NULL, FALSE);
384          }
385          return 0;
```

We check that the message came from our trackbar by comparing `lParam` with its handle. The new alpha is whatever `TBM_GETPOS` says. We replace the top byte of the primary colour [line 381], update the label and repaint. The next stroke you draw uses the new alpha automatically, because `Tool_MouseDown` reads `ts->primary` every time.

The slider only controls the primary colour. The secondary starts out solid. Swap carries alpha along with the colour though, so after a swap the secondary can end up see-through. Right clicking a quick colour makes it solid again.

### Colouring the label

```text
386      case WM_CTLCOLORSTATIC:
387          // Static controls ask their parent what colours to use. We make the
388          // label dark grey on the same colour as the palette.
389          SetTextColor((HDC)wParam, RGB(60, 60, 60));
390          SetBkColor((HDC)wParam, GetSysColor(COLOR_BTNFACE));
391          return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
```

A static text control asks its parent which colours to use, with `WM_CTLCOLORSTATIC`, just before it draws itself. We set dark grey text on the standard button face colour, and return a brush of that colour so the whole background of the label matches the palette. If you forget this, the label gets a white or mismatched box behind its text.

### The Swap button

```text
156  void
157  Palette_SwapColors(HWND palette)
158  {
159      Palette *p = (Palette *)GetWindowLongPtrW(palette, GWLP_USERDATA);
160      uint32_t tmp = p->settings->primary;
161      p->settings->primary = p->settings->secondary;
162      p->settings->secondary = tmp;
163      Palette_Refresh(palette);
164  }
```

The Swap button sends `WM_COMMAND`, and the palette's window procedure catches it before passing tool button clicks on to the main window. Swapping is three lines on the settings. Then `Palette_Refresh` moves the slider to the new primary alpha, fixes the label and repaints. `Palette_SwapColors` and `Palette_Refresh` are public (they're in `palette.h`), so the main window can call them too. Nobody outside calls them yet. `Palette_Refresh` gets its first outside caller in chapter 11, when the colour picker tool starts changing the primary colour from the canvas.

## An analogy

An artist mixes paint on a palette. The primary colour is what's on the brush. The secondary colour is a second pot kept handy. Alpha is how much water you've added. At 255 it's neat paint straight from the tube. At 128 it's half and half, and whatever's underneath shows through. At 0 you're painting with water and nothing happens. Swap just picks up the other pot.

## Adding functionality

Two small experiments.

First, change the last quick colour from magenta to orange. In `g_quickColors` at the bottom right, replace `RGB(255, 0, 255)` with `RGB(255, 165, 0)`.

Second, in `Palette_PickColor` take `CC_FULLOPEN` out of the flags, so `cc.Flags = CC_RGBINIT;`. Now the dialog opens in its smaller form, with a "Define Custom Colors" button that opens up the rest.

## Exercise

Make the **X** key swap the two colours, the way a lot of paint programs do.

*Hint: Add a new id to `resource.h`, a line like `"X", IDM_COLOR_SWAP, VIRTKEY` to the `IDA_MAINACCEL` table in `drawlite.rc`, and a `case` in `MainWindow_OnCommand` that calls `Palette_SwapColors(mw->hPalette)`.*

## That's it

We now have colours, see-through paint and a proper colour dialog. Next up are the shape tools. Lines, rectangles and ellipses, all of them built on the `Edit` from chapter 8.

[Chapter 10: Shapes](../10-shapes/README.md)
