# Chapter 4 - Housekeeping

[< Chapter 3: Resources](../03-resources/README.md)

Nothing new appears on screen in this chapter. The program looks and behaves the same as chapter 3. What changes is how the code is laid out, and that matters, because from here on the program grows every chapter. Tidy it now and you'll thank yourself later.

In this lesson you will

- split the program into several source files,
- keep a window's state in a struct tied to the window,
- see how CMake and the compiler warnings are set up,
- learn the comment and header conventions used in the rest of the code,
- find out how the repo and its GitHub Actions build are organised.

## Before we begin

Compared to chapter 3, `src/main.c` has shrunk, and four files are new: `src/mainwindow.c`, `src/mainwindow.h`, `src/aboutdlg.c` and `src/aboutdlg.h`. `CMakeLists.txt` lists them. The only change in `res` and `resource.h` is the version number, which went from 0.3 to 0.4. Build and run as usual.

```text
cmake -S . -B build && cmake --build build
```

![The same window as before, now with tidier code behind it](images/screenshot.png)

Same window, same menu, same About box. If it works exactly like chapter 3, we did it right.

## Splitting it up

In chapter 3 everything lived in `main.c`: the entry point, the window procedure and the dialog. That was fine for 120 lines. It won't be fine for 5,000. So each part of the program gets its own pair of files.

| File | What it does |
|------|--------------|
| `main.c` | Starts the program and runs the message loop. Nothing else |
| `mainwindow.c` and `.h` | The main window: its class, its state and its messages |
| `aboutdlg.c` and `.h` | The About box |

The rule is that a `.h` file says what a module offers, and the `.c` file holds how it does it. Other files only `#include` the header. Look at how small `wWinMain` has become.

```text
12  // Function: wWinMain
13  // The entry point of the program. It creates the main window and then
14  // runs the message loop until the window is closed.
15  //
16  // Returns:
17  //   The exit code that was passed to PostQuitMessage, or 1 if we could not start.
18  int WINAPI
19  wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
20  {
21      MSG msg;
22      MainWindow *mainWindow;
23      HACCEL hAccel;
24
25      UNREFERENCED_PARAMETER(hPrevInstance);
26      UNREFERENCED_PARAMETER(lpCmdLine);
27
28      mainWindow = MainWindow_Create(hInstance, nCmdShow);
29      if (!mainWindow)
30          return 1;
31
32      hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDA_MAINACCEL));
33
34      while (GetMessageW(&msg, NULL, 0, 0) > 0)
35      {
36          if (!TranslateAcceleratorW(MainWindow_GetHwnd(mainWindow), hAccel, &msg))
37          {
38              TranslateMessage(&msg);
39              DispatchMessageW(&msg);
40          }
41      }
42
43      MainWindow_Destroy(mainWindow);
44      return (int)msg.wParam;
45  }
```

It asks `MainWindow_Create` for a window [line 28], runs the message loop, and cleans up [line 43]. It doesn't know about window classes or menus any more. That's the whole point.

### Header files

Here's the header for the main window.

```text
 7  #ifndef MAINWINDOW_H
 8  #define MAINWINDOW_H
 9
10  #include <windows.h>
11
12  // Struct: MainWindow
13  // Everything the main window needs to remember. Callers should treat this
14  // as opaque and use the MainWindow_ functions instead of poking inside.
15  typedef struct MainWindow
16  {
17      HINSTANCE hInstance;    // The application instance
18      HWND hwnd;              // The window itself
19  } MainWindow;
20
21  // Function: MainWindow_Create
22  // Registers the window class (once), creates the window and shows it.
23  //
24  // Parameters:
25  //   hInstance - Instance handle from wWinMain
26  //   nCmdShow  - How the window should be shown (SW_SHOW, SW_MAXIMIZE, ...)
27  //
28  // Returns:
29  //   A new MainWindow, or NULL on failure. Free it with MainWindow_Destroy.
30  MainWindow *MainWindow_Create(HINSTANCE hInstance, int nCmdShow);
31
32  // Function: MainWindow_Destroy
33  // Frees the memory used by a MainWindow. The window itself is
34  // already gone by the time the message loop ends.
35  void MainWindow_Destroy(MainWindow *mw);
```

Two things to note. First, the `#ifndef MAINWINDOW_H` and `#define` lines at the top, and the `#endif` at the bottom, are an **include guard**. If two files both include this header, and one includes the other, the compiler would see the definitions twice and complain. The guard makes the second look a no-op.

Second, the comments. Every public function has a block that starts with `// Function:`, then says what it does, then lists its parameters and what it returns. We'll use that same pattern for the rest of the tutorial. The blocks live in the header, because that's where someone using the module looks first. In the `.c` file, we only repeat the block on private (`static`) functions, so that a reader of the code finds it right above the function.

The struct says "treat this as opaque", but you'll notice that it's defined right there in the header. C can hide a struct's fields completely, by only declaring it in the header. We don't, because it would mean more code for no gain in a tutorial. It's a polite request, not a lock.

## Per-window state

In chapter 2 we met `GWLP_USERDATA`, and used it to keep a click counter. Now we do it for real. The struct `MainWindow` holds what the window needs to remember. At the moment that's just the instance handle and the window handle. As we add features, it'll grow.

```text
21  MainWindow *
22  MainWindow_Create(HINSTANCE hInstance, int nCmdShow)
23  {
24      MainWindow *mw;
25
26      if (!MainWindow_RegisterClass(hInstance))
27          return NULL;
28
29      // calloc gives us zeroed memory, so every field starts as 0 or NULL
30      mw = (MainWindow *)calloc(1, sizeof(MainWindow));
31      if (!mw)
32          return NULL;
33      mw->hInstance = hInstance;
34
35      // The last parameter is handed to our WndProc in WM_NCCREATE
36      mw->hwnd = CreateWindowExW(0, MAINWINDOW_CLASS, L"DrawLite",
37          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
38          NULL, NULL, hInstance, mw);
39      if (!mw->hwnd)
40      {
41          free(mw);
42          return NULL;
43      }
44
45      ShowWindow(mw->hwnd, nCmdShow);
46      UpdateWindow(mw->hwnd);
47      return mw;
48  }
```

Walk through it. `MainWindow_RegisterClass` runs first [line 26]. Then `calloc` gives us a block of zeroed memory for the struct [line 30], so every field starts as zero or `NULL` without us writing it out. We pass the pointer as the last argument of `CreateWindowExW` [line 38], exactly as in chapter 2.

Then the window procedure picks it up again.

```text
 90  // Function: MainWindow_WndProc
 91  // The window procedure. Every message sent to our window passes through here.
 92  // It finds our MainWindow struct again and hands over to the right function.
 93  static LRESULT CALLBACK
 94  MainWindow_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
 95  {
 96      MainWindow *mw = (MainWindow *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
 97
 98      switch (msg)
 99      {
100      case WM_NCCREATE:
101          mw = (MainWindow *)((CREATESTRUCTW *)lParam)->lpCreateParams;
102          SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)mw);
103          mw->hwnd = hwnd;
104          break; // let DefWindowProc finish creating the window
105      case WM_COMMAND:
106          MainWindow_OnCommand(mw, LOWORD(wParam));
107          return 0;
108      case WM_DESTROY:
109          PostQuitMessage(0);
110          return 0;
111      }
112      return DefWindowProcW(hwnd, msg, wParam, lParam);
113  }
```

In `WM_NCCREATE` [lines 100 to 104] we fish the pointer out of the `CREATESTRUCTW`, store it with `SetWindowLongPtrW`, and also record the `HWND` in the struct. We use `break` rather than `return`, so that `DefWindowProcW` at the bottom [line 112] carries on and finishes creating the window. Every other message finds the struct with `GetWindowLongPtrW` [line 96].

One thing you'll notice is that `mw` is `NULL` for any message that arrives before `WM_NCCREATE`. Right now we don't handle any of those, so we get away with it. Later chapters check for `NULL` where it matters.

The win is that there are no globals. Chapter 3 had `g_hInstance`. Now the instance handle lives in the struct, and `MainWindow_OnCommand` passes it to the About box [line 135]. If we ever had two main windows, each would have its own state.

The struct is freed in `MainWindow_Destroy` [lines 50 to 54], which `wWinMain` calls once the message loop has ended. Later in the tutorial, the state gets freed when the window is destroyed, in `WM_NCDESTROY`. Here the window is long gone when we free it, so it's safe either way.

Notice also that `MainWindow_OnCommand` [line 121] takes the struct instead of an `HWND`. The window procedure's only job is to find the state and pass the message on to a function with a good name. As the number of messages grows, each one gets its own small function and the `switch` stays readable.

## The About box

```text
12  static INT_PTR CALLBACK AboutDlg_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
13
14  void
15  AboutDlg_Show(HINSTANCE hInstance, HWND hParent)
16  {
17      DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_ABOUT), hParent, AboutDlg_Proc, 0);
18  }
```

`AboutDlg_Show` hides the `DialogBoxParamW` call from chapter 3. Its dialog procedure is `static`, and nobody else can reach it. Notice we don't need a global instance handle any more. The caller passes it in.

## CMake

All three source files are listed in `CMakeLists.txt`.

```text
 7  # WIN32 makes this a GUI program (no console window)
 8  add_executable(drawlite WIN32
 9      src/main.c
10      src/mainwindow.c
11      src/aboutdlg.c
12      res/drawlite.rc
13  )
```

`WIN32` here makes it a GUI program, like `/SUBSYSTEM:WINDOWS` in chapter 1. The `RC` in `project(DrawLite C RC)` turns on the resource compiler. If you add a new file, add its name to this list and nothing more. CMake works out the rest.

The definitions come next.

```text
18  # We use the Unicode flavour of the Windows API, and target Windows 10 and up
19  target_compile_definitions(drawlite PRIVATE UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
```

`UNICODE` and `_UNICODE` pick the wide versions of the API, as we discussed in chapter 1. `WINVER` and `_WIN32_WINNT` say "Windows 10 and up". Without them, the headers hide the newer functions.

## Warnings

```text
25  if(MSVC)
26      target_compile_options(drawlite PRIVATE /W4 /utf-8)
27      # Our manifest is inside the .rc file, so stop the linker making its own
28      target_link_options(drawlite PRIVATE /MANIFEST:NO)
29  else()
30      target_compile_options(drawlite PRIVATE -Wall -Wextra)
31      # wWinMain needs -municode with MinGW
32      target_link_options(drawlite PRIVATE -municode)
33  endif()
```

Compilers can spot a lot of mistakes if you ask them to. `/W4` (Visual Studio) and `-Wall -Wextra` (GCC) turn on most of the useful warnings. They'll tell you about unused variables, comparisons between signed and unsigned numbers, and so on. We keep every chapter at zero warnings. That's why `UNREFERENCED_PARAMETER` shows up in `wWinMain` [lines 25 and 26]. If a warning is allowed to stay, you stop reading them, and then you'll miss the one that matters.

`/utf-8` tells Visual Studio that our source files are UTF-8, so that `L"..."` strings with unusual characters come out right. `-municode` is the MinGW switch we saw in chapter 1. And `/MANIFEST:NO` is the one from chapter 3.

## The repo

The tutorial lives in one git repo. Here's the layout.

```text
LICENSE                     MIT licence
.gitignore                  keeps build output (build/, *.exe, *.obj) out of git
.github/workflows/build.yml the automatic build, explained below
chapters/
    01-hello-win32/
    02-a-window-of-your-own/
    ...
        CMakeLists.txt      builds this chapter on its own
        src/                the C code
        res/                icon, manifest and .rc file (from chapter 3)
        README.md           the lesson you are reading
```

Each chapter folder is complete. You can copy one out of the repo and build it anywhere. That's the price of not using git tags like the old tutorial did. We repeat files a lot, but a reader never has to check out an old version of anything.

## Automatic builds

GitHub can build the code every time someone pushes a change. You set it up with a file in `.github/workflows`. Ours is `build.yml`, and it's short.

```text
 1  name: Build every chapter
 2
 3  on:
 4    push:
 5    pull_request:
 6
 7  jobs:
 8    build:
 9      runs-on: windows-latest
10      strategy:
11        fail-fast: false
12        matrix:
13          toolchain: [msvc, mingw]
14      steps:
15        - uses: actions/checkout@v4
16
17        - name: Build all chapters
18          shell: bash
19          run: |
20            set -e
21            for dir in chapters/*/; do
22              name=$(basename "$dir")
23              echo "::group::$name ($TOOLCHAIN)"
24              if [ "$TOOLCHAIN" = "msvc" ]; then
25                cmake -S "$dir" -B "build/$TOOLCHAIN/$name"
26                cmake --build "build/$TOOLCHAIN/$name" --config Release
27              else
28                cmake -S "$dir" -B "build/$TOOLCHAIN/$name" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
29                cmake --build "build/$TOOLCHAIN/$name"
30              fi
31              echo "::endgroup::"
32            done
33          env:
34            TOOLCHAIN: ${{ matrix.toolchain }}
```

Here's what it says, in plain English.

- `on: push` and `pull_request` mean "run this whenever somebody pushes code, or proposes a change".
- `runs-on: windows-latest` asks GitHub for a Windows machine for a few minutes. It's a fresh one each time, so nothing is left behind from the last run.
- `matrix: toolchain: [msvc, mingw]` runs the whole job twice, once for each compiler. `fail-fast: false` means that if one fails, the other still finishes, so you can see which compiler is unhappy.
- `actions/checkout` downloads our code onto that machine.
- The `run` block is a small shell script. It goes through every folder in `chapters/`, and for each one runs `cmake -S` to configure the build and `cmake --build` to compile it. `set -e` stops the script at the first error, which makes the whole job go red.
- MSVC is built in `Release`. With MinGW we ask for the `MinGW Makefiles` generator.

That's all there is to it. If you break a chapter, the run goes red and GitHub tells you. A green tick on the repo means every chapter builds with both compilers. That doesn't mean it runs perfectly. The build only checks that it compiles and links, and I tested the running programs with MinGW and Wine.

## Adding functionality

Let's put the struct to work. Add a counter to `MainWindow` in `mainwindow.h`.

```c
    int menuClicks;         // How many menu or shortcut commands we've handled
```

Then, at the top of `MainWindow_OnCommand` in `mainwindow.c`, right under the opening brace [line 123], add

```c
    mw->menuClicks++;
```

Nothing shows on screen yet. We'll display it in the exercise.

## Exercise

Show `menuClicks` in the About box text, or in a message box when someone picks File > New.

*Hint: `StringCchPrintfW` from `<strsafe.h>` builds the string, like in chapter 2. Pass the struct to the place that needs it.*

## That's it

Whew. A whole chapter and no new feature. But the program now has a shape that can carry twenty chapters. Next up, a toolbar and a status bar.

[Chapter 5: Toolbar and status bar](../05-toolbar-and-status-bar/README.md)
