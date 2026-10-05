# Chapter 15 - PNG, JPEG and the clipboard

[< Chapter 14: Open and save](../14-open-and-save-bmp/README.md)

Last time we saved pictures as `.bmp`. They work, but a BMP of a photo is enormous, and nobody sends them to anybody. This chapter lets DrawLite open and save PNG and JPEG files, and copy and paste pictures through the clipboard. To do it, we'll have to talk to COM. It sounds scary. It isn't, once you see what it really is.

In this lesson you will

- meet COM, and see that in C an interface is just a struct of function pointers,
- load and save PNG and JPEG with the Windows Imaging Component (WIC),
- copy a picture to the clipboard in two formats at once,
- paste a picture from another program.

## Before we begin

New files this time are `src/imageio.c` and `src/imageio.h` (WIC), and `src/clipboard.c` and `src/clipboard.h`. `main.c` starts and stops WIC, `filedlg.c` gets new file type filters, and `mainwindow.c` now calls `ImageIO_Load` and `ImageIO_Save` in place of the BMP functions. The Edit menu has two new items, **Copy** (`Ctrl+C`) and **Paste as New Image** (`Ctrl+V`). In `CMakeLists.txt` we link a few more libraries: `ole32`, `oleaut32`, `windowscodecs` and `uuid`.

Build and run it as usual (see [chapter 1](../01-hello-win32/README.md) if you need a reminder).

```text
cmake -S . -B build && cmake --build build
```

![DrawLite saving a drawing as PNG](images/screenshot.png)

Draw something and press `Ctrl+Shift+S`. The Save dialog now offers PNG, JPEG and Bitmap. Try all three and compare the sizes in Explorer. Then press `Ctrl+C`, open Paint or a chat program, and paste. It works the other way too: copy a picture out of a web page and press `Ctrl+V` in DrawLite.

## COM, in C

Windows has an enormous number of features that are offered as **COM objects** (Component Object Model). WIC is one of them. COM was designed to be used from C++, but underneath it's plain C, and we can use it that way.

Here's the whole idea. A COM object is a pointer to something. That something starts with a pointer to a table of function pointers (called the *vtable*). You call a method by going through the table and passing the object itself as the first argument. In C that's written like this.

```c
// What every COM call looks like if you do it by hand
HRESULT hr = factory->lpVtbl->CreateStream(factory, &stream);
```

That's ugly and easy to get wrong, so Windows provides macros that do it for us. If you define `COBJMACROS` before you include the headers, you get one macro for every method. They're named *Interface*_*Method*, and you pass the object as the first argument.

```text
 7  #define COBJMACROS          // Gives us IFoo_Method(obj, ...) macros for calling COM from C
 8  #include <windows.h>
 9  #include <objbase.h>
10  #include <wincodec.h>
```

```c
// The same call, with COBJMACROS
HRESULT hr = IWICImagingFactory_CreateStream(factory, &stream);
```

You'll see this pattern for the rest of the chapter. Three more things to know.

**HRESULT.** Almost every COM method returns a number called an `HRESULT`. Zero or positive means it worked, negative means it failed. You don't compare it with anything. You use `SUCCEEDED(hr)` and `FAILED(hr)`. It's why you'll see lots of code that looks like `if (SUCCEEDED(hr)) hr = NextThing(...)`. If one step fails, all the following ones are skipped, and `hr` holds the first error.

**Release.** Every COM object keeps a count of how many people are using it. When you get a pointer, the count is one. When you are done you call `Release` on it, and the count goes down. When it reaches zero, the object frees itself. If you forget, you leak memory. If you call it twice, you crash. So every object you receive gets exactly one `Release`. (If you hand a pointer to someone else to keep, you call `AddRef` first. We don't need to here.)

**Out parameters.** Methods that create things give them back through a pointer to a pointer, like `&stream` above. You start with `NULL`, call the method, and if it worked, you have an object.

Don't worry if this is hazy. We'll use the same three ideas, again and again, in this chapter.

### CoInitializeEx and CoCreateInstance

Before you can use COM, each thread has to say so.

```c
HRESULT CoInitializeEx(LPVOID pvReserved,     // Always NULL
                       DWORD dwCoInit);       // COINIT_APARTMENTTHREADED for a thread with windows
void CoUninitialize(void);                    // The matching undo. One per successful CoInitializeEx
```

`COINIT_APARTMENTTHREADED` is the one for programs with a user interface, where every call happens on the one UI thread. Then we ask Windows to make us a WIC factory, which is the object that makes all the other WIC objects.

```c
HRESULT CoCreateInstance(REFCLSID rclsid,      // Which kind of object. A GUID that names the class
                         LPUNKNOWN pUnkOuter,  // NULL. Only for aggregation, which we don't do
                         DWORD dwClsContext,   // CLSCTX_INPROC_SERVER: the code is a DLL in our process
                         REFIID riid,          // Which interface we want. Another GUID
                         LPVOID *ppv);         // Receives the interface pointer
```

```text
31  BOOL
32  ImageIO_Init(void)
33  {
34      HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
35      // S_FALSE means "already initialised on this thread". That is fine too.
36      g_comStarted = SUCCEEDED(hr);
37
38      hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
39          &IID_IWICImagingFactory, (void **)&g_factory);
40      return SUCCEEDED(hr);
41  }
```

A GUID is a 128 bit number that is unique in the whole world, which is how COM names things without anybody having to agree on a name. In C++ you can pass the GUIDs themselves, but in C you pass their addresses, hence `&CLSID_WICImagingFactory`. The actual numbers live in the `uuid` library we added to the CMake file.

```text
43  void
44  ImageIO_Shutdown(void)
45  {
46      if (g_factory)
47      {
48          IWICImagingFactory_Release(g_factory);
49          g_factory = NULL;
50      }
51      if (g_comStarted)
52      {
53          CoUninitialize();
54          g_comStarted = FALSE;
55      }
56  }
```

When we shut down we release the factory first, and then call `CoUninitialize`, but only if `CoInitializeEx` succeeded. `main.c` calls `ImageIO_Init` first thing in `wWinMain`, and `ImageIO_Shutdown` after the message loop. If WIC isn't available, the program says so and quits.

## Loading a picture

The Windows Imaging Component knows how to read PNG, JPEG, GIF, TIFF, BMP and more. We give it a file and it gives us a *decoder*. A decoder has *frames* (a GIF can have many, a PNG has one), and we take the first.

```text
129          return NULL;
130      }
131      // WIC looks inside the file to see what it is, so a PNG called .jpg still loads
132      hr = IWICImagingFactory_CreateDecoderFromFilename(g_factory, path, NULL, GENERIC_READ,
133          WICDecodeMetadataCacheOnDemand, &decoder);
134      if (FAILED(hr))
135      {
136          ImageIO_Fail(error, errorLen, L"This file could not be opened as a picture.", hr);
137          return NULL;
138      }
139      s = ImageIO_FromDecoder(decoder, error, errorLen);
140      IWICBitmapDecoder_Release(decoder);
141      return s;
```

`CreateDecoderFromFilename` doesn't go by the file's extension. It looks inside the file, so a PNG that's been renamed to `.jpg` still loads. If that fails, we build an error message that includes the `HRESULT` in hex, which helps when somebody sends you a bug report.

Next, `ImageIO_FromDecoder` turns a frame into a `Surface`.

```text
84      // Pictures come in all sorts of pixel formats: 8 bit grey, 16 bit colour,
85      // palettes and so on. The format converter turns any of them into the one
86      // we use: 32 bits, blue first, premultiplied. Yes, WIC speaks premultiplied too.
87      hr = IWICImagingFactory_CreateFormatConverter(g_factory, &converter);
88      if (SUCCEEDED(hr))
89          hr = IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame,
90              &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0,
91              WICBitmapPaletteTypeCustom);
```

Pictures come in all sorts of pixel formats: grey, palettes, 16 bits per channel, CMYK. We don't want to deal with any of them. So we use a *format converter*, which takes any frame and presents it as something else. We say `GUID_WICPixelFormat32bppPBGRA`, which reads as 32 bits, premultiplied, blue then green then red then alpha. That's exactly the pixel layout from [chapter 7](../07-pixels-you-own/README.md), so we can have WIC write straight into our surface.

```text
 98      s = Surface_Create((int)w, (int)h);
 99      if (!s)
100      {
101          ImageIO_Fail(error, errorLen, L"There is not enough memory for an image this big.", E_OUTOFMEMORY);
102          goto done;
103      }
104      hr = IWICFormatConverter_CopyPixels(converter, NULL, w * 4, w * h * 4, (BYTE *)s->pixels);
```

`CopyPixels` takes the stride (bytes per row, which here is just `w * 4`), the size of the buffer, and the buffer. And that's the whole decoder. Compare this to the 150 lines of `Bmp_Decode`.

Notice the `goto done` at the end of the function. When a function has to release several things on every path, one cleanup label at the bottom is much tidier than repeating the `Release` calls in each `if`. It's one of the few places where `goto` is a good idea.

### Loading from memory

The clipboard gives us PNG data in memory, not in a file. WIC can read from a stream, and it can make a stream from a block of memory.

```text
152      if (!g_factory)
153          return NULL;
154      hr = IWICImagingFactory_CreateStream(g_factory, &stream);
155      if (SUCCEEDED(hr))
156          hr = IWICStream_InitializeFromMemory(stream, (BYTE *)data, (DWORD)size);
157      if (SUCCEEDED(hr))
158          hr = IWICImagingFactory_CreateDecoderFromStream(g_factory, (IStream *)stream, NULL,
159              WICDecodeMetadataCacheOnDemand, &decoder);
160      if (FAILED(hr))
161          ImageIO_Fail(error, errorLen, L"The picture could not be read.", hr);
162      else
163      {
164          s = ImageIO_FromDecoder(decoder, error, errorLen);
165          IWICBitmapDecoder_Release(decoder);
166      }
167      if (stream)
168          IWICStream_Release(stream);
169      return s;
170  }
```

Same idea, different front door. The decoder is made from the stream, and then `ImageIO_FromDecoder` does the rest. Note the careful `Release` calls: the decoder only if we got one, the stream always.

## Saving a picture

Saving is more work than loading, because we have to choose.

```text
187      // Our pixels are premultiplied. PNG and JPEG files are not, so convert.
188      // JPEG has no transparency at all, so we put the picture on white paper.
189      buffer = (uint8_t *)malloc((size_t)stride * s->height);
190      if (!buffer)
191          return E_OUTOFMEMORY;
192      for (i = 0; i < count; i++)
193      {
194          uint32_t p = s->pixels[i];
195          if (jpeg)
196          {
197              uint32_t over = 255 - PIX_A(p);     // How much white shows through
198              buffer[i * 3 + 0] = (uint8_t)(PIX_B(p) + over);
199              buffer[i * 3 + 1] = (uint8_t)(PIX_G(p) + over);
200              buffer[i * 3 + 2] = (uint8_t)(PIX_R(p) + over);
201          }
202          else
203          {
204              uint32_t q = Pixel_Unpremultiply(p);
205              memcpy(buffer + i * 4, &q, 4);      // 0xAARRGGBB is B, G, R, A in memory
206          }
207      }
```

Our pixels are premultiplied, and PNG and JPEG files aren't. So for PNG we call `Pixel_Unpremultiply` first (we did the same in chapter 14). JPEG has no transparency at all. If we just threw away the alpha, a transparent pixel would turn black. So we pretend the picture is lying on white paper. For a premultiplied pixel that's a neat bit of arithmetic. The colour is already scaled by alpha, so the white that shows through is `255 - alpha`, and we simply add it to each channel.

Then we create an encoder and walk through the steps.

```text
209      hr = IWICImagingFactory_CreateEncoder(g_factory, container, NULL, &encoder);
210      if (SUCCEEDED(hr))
211          hr = IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
212      if (SUCCEEDED(hr))
213          hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &props);
214
215      if (SUCCEEDED(hr) && jpeg)
216      {
217          // Quality 0.0 to 1.0. We always use 90%.
218          PROPBAG2 option;
219          VARIANT value;
220
221          ZeroMemory(&option, sizeof(option));
222          option.pstrName = (LPOLESTR)L"ImageQuality";
223          VariantInit(&value);
224          value.vt = VT_R4;
225          value.fltVal = 0.9f;
226          IPropertyBag2_Write(props, 1, &option, &value);
227      }
228      if (SUCCEEDED(hr))
229          hr = IWICBitmapFrameEncode_Initialize(frame, props);
230      if (SUCCEEDED(hr))
231          hr = IWICBitmapFrameEncode_SetSize(frame, (UINT)s->width, (UINT)s->height);
232      if (SUCCEEDED(hr))
233      {
234          // We ask for a pixel format. The encoder changes it if it cannot do that one.
235          WICPixelFormatGUID wanted = format;
236          hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
237          if (SUCCEEDED(hr) && !IsEqualGUID(&wanted, &format))
238              hr = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
239      }
240      if (SUCCEEDED(hr))
241          hr = IWICBitmapFrameEncode_WritePixels(frame, (UINT)s->height, stride, stride * (UINT)s->height, buffer);
242      if (SUCCEEDED(hr))
243          hr = IWICBitmapFrameEncode_Commit(frame);
244      if (SUCCEEDED(hr))
245          hr = IWICBitmapEncoder_Commit(encoder);
```

Create the encoder for PNG or JPEG, point it at a stream, make a frame, give it a size and a pixel format, write the pixels, and `Commit` the frame and then the encoder. The `Commit` calls are what finish the file. JPEG also gets a quality setting through a property bag. We always use 0.9, or 90%, because this program doesn't have a place to ask. (Adding a quality slider would be a nice exercise, though not this one.)

`SetPixelFormat` is odd. You tell it the format you'd *like*, and it changes the variable to what it can actually do. We compare afterwards, and fail if it wasn't what we asked for.

### Choosing by extension

```text
280      if (ImageIO_HasExtension(path, L".bmp"))
281          return Bmp_Save(s, path, error, errorLen);
282
283      jpeg = ImageIO_HasExtension(path, L".jpg") || ImageIO_HasExtension(path, L".jpeg");
284      if (!jpeg && !ImageIO_HasExtension(path, L".png"))
285      {
286          ImageIO_Fail(error, errorLen, L"DrawLite can save .png, .jpg and .bmp files.", E_INVALIDARG);
287          return FALSE;
288      }
289      if (!g_factory)
290          return FALSE;
291
292      hr = IWICImagingFactory_CreateStream(g_factory, &stream);
293      if (SUCCEEDED(hr))
294          hr = IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE);
295      if (SUCCEEDED(hr))
296          hr = ImageIO_Encode(s, jpeg ? &GUID_ContainerFormatJpeg : &GUID_ContainerFormatPng, jpeg, (IStream *)stream);
297      // Releasing the stream closes the file
298      if (stream)
299          IWICStream_Release(stream);
300      if (FAILED(hr))
301      {
302          ImageIO_Fail(error, errorLen, L"The file could not be saved.", hr);
303          return FALSE;
304      }
305      return TRUE;
```

`.bmp` goes to our own writer from the last chapter. `.png` and `.jpg` go to WIC. Anything else is an error that tells you what it *can* save. The stream is a WIC file stream. We don't call `CloseHandle`. Releasing the stream closes the file, which is the COM way.

## The common dialogs, again

`filedlg.c` has two new filters. The Open one has an "All pictures" entry that lists every extension WIC understands. The Save one lists PNG, JPEG and Bitmap, starting on PNG.

```text
12  // A filter is a list of pairs: the text the user sees, then the pattern.
13  // Each piece ends with a \0, and the whole list ends with an extra \0.
14  // That is why this is not a normal string, and why we cannot use strlen on it.
15  #define OPEN_FILTER L"All pictures\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0" \
16                      L"PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg;*.jpeg\0Bitmap (*.bmp)\0*.bmp\0" \
17                      L"All files (*.*)\0*.*\0"
18  #define SAVE_FILTER L"PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg;*.jpeg\0Bitmap (*.bmp)\0*.bmp\0"
```

There's a small problem. Last chapter we set `lpstrDefExt` so a name typed without an extension got `.bmp` added. But now the extension should depend on which filter is selected, and `lpstrDefExt` can only hold one. So `FileDlg_SaveAs` does it by hand: after the dialog closes, it looks at `nFilterIndex` (which entry was selected, counting from 1) and appends `.png`, `.jpg` or `.bmp`. And because the dialog's own "file already exists" check ran on the name without the extension, we ask again ourselves if the full name exists.

One more limit to know about. If you type a name with a dot in it but no real extension ("sunset.final"), the code decides you've given an extension, and `ImageIO_Save` will say it can't save that. It's an honest limitation, and you'll see the error message rather than a lost picture.

`MainWindow_Save` has a small change too. If the document's name is something we can't write (you opened a `.gif`, say), `ImageIO_CanSave` fails and Save quietly turns into Save As.

## The clipboard

The clipboard is a shelf where programs leave things for each other. The clever part is that you leave the same thing in *several formats* at once. The program that copies offers everything it can make, and the program that pastes takes the best one it understands. A format is just a number. `CF_DIB` and `CF_DIBV5` are built in, and for PNG we register a name.

```text
20  static UINT
21  Clipboard_PngFormat(void)
22  {
23      // RegisterClipboardFormat gives the same number to everyone who asks for the same name
24      static UINT format = 0;
25      if (!format)
26          format = RegisterClipboardFormatW(L"PNG");
27      return format;
28  }
```

```c
UINT RegisterClipboardFormatW(LPCWSTR lpszFormat);   // A name. Everyone who asks for "PNG" gets the same number
```

### Putting a picture on the clipboard

Clipboard data has to live in a block of **global memory**, which is an old Win32 kind of memory that's moveable and can be handed from one program to another. The recipe is always the same.

```text
30  // Function: Clipboard_Put
31  // Copies a block of memory into a new global memory block and hands it to the
32  // clipboard. The clipboard owns the block after a successful SetClipboardData.
33  static BOOL
34  Clipboard_Put(UINT format, const void *data, size_t size)
35  {
36      HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
37      void *dst;
38
39      if (!mem)
40          return FALSE;
41      dst = GlobalLock(mem);
42      if (!dst)
43      {
44          GlobalFree(mem);
45          return FALSE;
46      }
47      memcpy(dst, data, size);
48      GlobalUnlock(mem);
49
50      if (!SetClipboardData(format, mem))
51      {
52          GlobalFree(mem);    // It did not take it, so it is still ours
53          return FALSE;
54      }
55      return TRUE;
56  }
```

```c
HGLOBAL GlobalAlloc(UINT uFlags,        // GMEM_MOVEABLE, which the clipboard requires
                    SIZE_T dwBytes);    // How many bytes
LPVOID GlobalLock(HGLOBAL hMem);        // Pins the block and gives you a pointer to it
BOOL GlobalUnlock(HGLOBAL hMem);        // Lets go of the pointer
HANDLE SetClipboardData(UINT uFormat,   // Which format this data is in
                        HANDLE hMem);   // The block. The clipboard owns it if this succeeds
```

We allocate, lock to get a pointer, copy the bytes in, and unlock. Then `SetClipboardData` takes it. Here is the rule that trips everyone up. If `SetClipboardData` succeeds, the block belongs to the clipboard and you mustn't free it. If it fails, it's still yours, and you do [line 52].

Now the whole copy.

```text
65      // Do the slow encoding before we open the clipboard. Nobody else can use
66      // it while we have it open.
67      png = ImageIO_EncodePng(s, &pngSize);
68      bmp = Bmp_Encode(s, &bmpSize);
69      if (!bmp)
70      {
71          free(png);
72          return FALSE;
73      }
74
75      if (OpenClipboard(owner))
76      {
77          EmptyClipboard();   // Throw away whatever was there. We now own the clipboard.
78          ok = TRUE;
79          if (png)
80              Clipboard_Put(Clipboard_PngFormat(), png, pngSize);
81
82          // A .bmp file is a file header followed by exactly what the clipboard
83          // wants, so we just skip the header. If the picture has transparency
84          // the data starts with a V5 header, which has its own format name.
85          {
86              const BITMAPINFOHEADER *ih = (const BITMAPINFOHEADER *)(bmp + sizeof(BITMAPFILEHEADER));
87              UINT format = ih->biSize >= sizeof(BITMAPV5HEADER) ? CF_DIBV5 : CF_DIB;
88              ok = Clipboard_Put(format, bmp + sizeof(BITMAPFILEHEADER), bmpSize - sizeof(BITMAPFILEHEADER));
89          }
90          CloseClipboard();
91      }
92      free(png);
93      free(bmp);
```

First we make the data, *before* opening the clipboard. While we have it open, every other program that wants it has to wait, so we don't want to do anything slow in there. Then `OpenClipboard`, `EmptyClipboard` (which makes us the owner), and one `Clipboard_Put` for each format. And always `CloseClipboard` afterwards.

We offer two formats. The first is PNG, which keeps transparency and is understood by most modern programs. The second is a DIB, which is the clipboard's name for a bitmap with no file header. And there's a lovely shortcut here. Our `Bmp_Encode` makes a whole `.bmp` file, which is a 14 byte file header followed by exactly what the clipboard wants. So we skip the first 14 bytes, and we're done. If the picture has transparency, `Bmp_Encode` wrote a `BITMAPV5HEADER` (that's why chapter 15 changes it from V4 to V5), and that goes on as `CF_DIBV5`. Otherwise it's `CF_DIB`.

### Pasting

Pasting is the opposite. We try the formats in order of quality: PNG first, then DIBV5, then DIB. The first one that works wins.

```text
150  Surface *
151  Clipboard_PasteImage(HWND owner, wchar_t *error, size_t errorLen)
152  {
153      Surface *s = NULL;
154      UINT formats[3];
155      int i;
156
157      formats[0] = Clipboard_PngFormat();     // Best: has transparency
158      formats[1] = CF_DIBV5;
159      formats[2] = CF_DIB;
160
161      if (!OpenClipboard(owner))
162      {
163          StringCchCopyW(error, errorLen, L"The clipboard is in use by another program.");
164          return NULL;
165      }
166      for (i = 0; i < 3 && !s; i++)
167      {
168          HGLOBAL mem;
169          const uint8_t *data;
170
171          if (!IsClipboardFormatAvailable(formats[i]))
172              continue;
173          mem = GetClipboardData(formats[i]);     // Still owned by the clipboard. Do not free.
174          if (!mem)
175              continue;
176          data = (const uint8_t *)GlobalLock(mem);
177          if (!data)
178              continue;
179          if (i == 0)
180              s = ImageIO_LoadMemory(data, GlobalSize(mem), error, errorLen);
181          else
182              s = Clipboard_DibToSurface(data, GlobalSize(mem), error, errorLen);
183          GlobalUnlock(mem);
184      }
185      CloseClipboard();
186
187      if (!s && error[0] == L'\0')
188          StringCchCopyW(error, errorLen, L"There is no picture on the clipboard.");
189      return s;
190  }
```

`GetClipboardData` gives you the clipboard's own block. Lock it, use it, unlock it, and **don't free it**. It still belongs to the clipboard. The PNG goes through `ImageIO_LoadMemory`. The DIBs need a little help, because they have no file header.

```text
124      // Where do the pixels start? After the header, then the colour table if
125      // there is one, then (for a plain 40 byte header only) the three colour masks.
126      pixelsAt = ih->biSize;
127      if (ih->biBitCount <= 8)
128          pixelsAt += (size_t)(ih->biClrUsed ? ih->biClrUsed : (1u << ih->biBitCount)) * 4;
129      if (ih->biCompression == BI_BITFIELDS && ih->biSize == sizeof(BITMAPINFOHEADER))
130          pixelsAt += 12;
131
132      file = (uint8_t *)malloc(headerLen + size);
133      if (!file)
134      {
135          StringCchCopyW(error, errorLen, L"Not enough memory.");
136          return NULL;
137      }
138      ZeroMemory(&fh, sizeof(fh));
139      fh.bfType = 0x4D42;
140      fh.bfSize = (DWORD)(headerLen + size);
141      fh.bfOffBits = (DWORD)(headerLen + pixelsAt);
142      memcpy(file, &fh, sizeof(fh));
143      memcpy(file + headerLen, dib, size);
144
145      s = Bmp_Decode(file, headerLen + size, error, errorLen);
146      free(file);
147      return s;
```

We build the 14 byte header ourselves and put it on the front. The only clever bit is `bfOffBits`, where the pixels start. That's the info header, plus the colour table if there is one, plus the three colour masks if it's a plain 40 byte header with `BI_BITFIELDS`. Then the reader we wrote in chapter 14 does the rest. The same function reads files, and it reads the clipboard.

### Paste as New Image

```text
531  // Function: MainWindow_Paste
532  // Makes a new image out of whatever picture is on the clipboard.
533  static void
534  MainWindow_Paste(MainWindow *mw)
535  {
536      wchar_t error[128] = L"";
537      Surface *surface;
538      Document *doc;
539
540      if (!Clipboard_HasImage())
541      {
542          MessageBoxW(mw->hwnd, L"There is no picture on the clipboard.", L"DrawLite", MB_OK | MB_ICONINFORMATION);
543          return;
544      }
545      if (!MainWindow_ConfirmDiscard(mw))
546          return;
547
548      surface = Clipboard_PasteImage(mw->hwnd, error, ARRAYSIZE(error));
549      if (!surface)
550      {
551          MessageBoxW(mw->hwnd, error, L"DrawLite", MB_OK | MB_ICONERROR);
552          return;
553      }
554      doc = Doc_CreateFromSurface(surface);
555      if (!doc)
556      {
557          Surface_Destroy(surface);
558          return;
559      }
560      doc->modified = TRUE;       // It has never been saved, so it has something to lose
561      MainWindow_SetDocument(mw, doc);
562  }
```

The menu says "Paste as New Image" because that's what it does. It replaces the current picture with the clipboard one. This is why we call `MainWindow_ConfirmDiscard` first. The new document is marked `modified`, because it has never been saved. There's no way to paste *into* the current picture yet. That needs layers, and layers are what we do in chapter 16.

## The restaurant order

Think of the clipboard as a hatch between two kitchens. When you copy, you put the same dish on the hatch in two different containers: a PNG, and a DIB. The person on the other side doesn't ask you what's on offer. They look at the hatch and take the container they prefer. If they only have plates for DIBs, they take that one, and if they prefer PNG they take that. And as for COM, it's the waiter who hands you a ticket for each thing you ask for. Give every ticket back (`Release`), or the waiters will run out of tickets.

## Adding functionality

Make JPEG quality something you can change. In `ImageIO_Encode`, the quality is a constant.

1. Add `static float g_jpegQuality = 0.9f;` near the top of `imageio.c`.
2. Replace `0.9f` in the `value.fltVal` line with `g_jpegQuality`.
3. Now you can set it to `0.3f` and look at how ugly your picture gets.

## Exercise

The Paste menu item is always enabled, even when there is nothing to paste. Grey it out when the clipboard holds no picture.

*Hint: Handle `WM_INITMENUPOPUP` in the main window's message handler, and call `EnableMenuItem` with `MF_GRAYED` or `MF_ENABLED`, depending on `Clipboard_HasImage()`.*

## That's it

You've now talked to COM, which scares plenty of programmers, and DrawLite reads and writes the formats people actually use. Next we'll add layers, which is how a real paint program works.

[Chapter 16: Layers](../16-layers/README.md)
