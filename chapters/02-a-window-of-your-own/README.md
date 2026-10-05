# Chapter 2 - A window of your own

[< Chapter 1: Hello, Win32](../01-hello-win32/README.md)

A message box is nice, but it isn't much of a program. In this chapter we make a real window, and find out how Windows talks to it.

In this lesson you will

- register a window class and create a window,
- learn what messages are and write a message loop to handle them,
- respond to a mouse click.

## Before we begin

Only two files changed since chapter 1. `src/main.c` has all the new code, and `CMakeLists.txt` is new. From here on we build with CMake. Open a terminal in this folder and type

```text
cmake -S . -B build && cmake --build build
```

then run `build/drawlite.exe`. If you haven't got a compiler yet, [chapter 1](../01-hello-win32/README.md) shows you three ways to get one. Visual Studio users can open the folder as before and press F5.

You should see an empty window. Click inside it.

![An empty window with a message box showing the click position](images/screenshot.png)

Whoa! That's a proper window. You can move it, resize it, minimise it and close it, and we wrote very little code for any of that.

## The code

The whole program is one file. Here is the entry point.

```text
21  // Windows entry point
22  int WINAPI
23  wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
24  {
25      MSG msg;            // MSG structure to store messages
26      WNDCLASSEXW wcx;    // Window class information
27      HWND hwndMain;      // Main window handle
28      AppState state = { 0 };
29
30      UNREFERENCED_PARAMETER(hPrevInstance);
31      UNREFERENCED_PARAMETER(lpCmdLine);
32
33      // Initialize the struct to zero
34      ZeroMemory(&wcx, sizeof(wcx));
35      wcx.cbSize = sizeof(WNDCLASSEXW);               // Must always be sizeof(WNDCLASSEXW)
36      wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // Class styles
37      wcx.lpfnWndProc = MainWndProc;                  // Pointer to the callback procedure
38      wcx.cbClsExtra = 0;                             // Extra bytes to allocate after the class
39      wcx.cbWndExtra = 0;                             // Extra bytes to allocate after each window
40      wcx.hInstance = hInstance;                      // Instance of the application
41      wcx.hIcon = NULL;                               // Class icon
42      wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);     // Class cursor
43      wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Background brush
44      wcx.lpszMenuName = NULL;                        // Menu resource
45      wcx.lpszClassName = L"DrawLite";                // Name of this class
46      wcx.hIconSm = NULL;                             // Small icon for this class
47
48      // Register this window class with Windows
49      if (!RegisterClassExW(&wcx))
50          return 1;
51
52      // Create the window
53      hwndMain = CreateWindowExW(
54          0,                                  // Extended window style
55          L"DrawLite",                        // Window class name
56          L"Chapter 2 - A window of your own",// Window title
57          WS_OVERLAPPEDWINDOW,                // Window style
58          CW_USEDEFAULT, CW_USEDEFAULT,       // (x,y) position of the window
59          CW_USEDEFAULT, CW_USEDEFAULT,       // Width and height of the window
60          NULL,                               // Parent window
61          NULL,                               // Menu
62          hInstance,                          // Application instance
63          &state);                            // Pointer passed to WM_NCCREATE and WM_CREATE
64
65      // Check if window creation was successful
66      if (!hwndMain)
67          return 1;
68
69      // Make the window visible
70      ShowWindow(hwndMain, nCmdShow);
71      UpdateWindow(hwndMain);
72
73      // Process messages coming to this window
74      while (GetMessageW(&msg, NULL, 0, 0) > 0)
75      {
76          TranslateMessage(&msg);
77          DispatchMessageW(&msg);
78      }
79
80      // Return value to the system
81      return (int)msg.wParam;
82  }
```

And here is the function that Windows calls whenever something happens to our window.

```text
 84  static LRESULT CALLBACK
 85  MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
 86  {
 87      // Messages arrive before WM_NCCREATE too, so this can be NULL
 88      AppState *state = (AppState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
 89
 90      switch (msg)
 91      {
 92      case WM_NCCREATE:
 93          {
 94              // The pointer we gave CreateWindowExW comes along in the CREATESTRUCT.
 95              // Park it in the window so we can pick it up again later.
 96              CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
 97              SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
 98              return DefWindowProcW(hwnd, msg, wParam, lParam);
 99          }
100      case WM_LBUTTONDOWN:
101          {
102              wchar_t str[100];
103              state->clicks++;
104              StringCchPrintfW(str, 100, L"Co-ordinates are\nX=%d and Y=%d\n\nClick number %d",
105                  GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), state->clicks);
106              MessageBoxW(hwnd, str, L"Left Button Clicked", MB_OK);
107          }
108          break;
109      case WM_DESTROY:
110          // User closed the window
111          PostQuitMessage(0);
112          break;
113      default:
114          // Call the default window handler
115          return DefWindowProcW(hwnd, msg, wParam, lParam);
116      }
117      return 0;
118  }
```

Whew. That looks long, but it has only three jobs. It sets up a window class, makes a window, and then loops around waiting for messages. Let's take them one at a time.

## Messages

Before the code, we need the one big idea of Windows programming. Your program doesn't run from the top to the bottom and finish. It sits and waits. When the user moves the mouse, presses a key, or drags the window's edge, Windows puts a small note in a queue for your program. That note is a **message**. Your program picks them up one at a time and decides what to do about each.

Think of a restaurant. You are the customer (the user) and you tell the waiter (Windows) what you want. The waiter writes your order on a slip and drops it into the kitchen's queue. The cook (our program) takes slips off the queue one at a time and prepares whatever each one says. A slip might say "one soup" or "one steak". For us, a slip might say `WM_LBUTTONDOWN` (the left mouse button went down) or `WM_DESTROY` (the window is being destroyed). If the cook doesn't recognise a slip, he doesn't throw a fit. He hands it to the head chef, who has a sensible default for everything. That head chef is `DefWindowProcW`, and we'll meet it in a moment.

Each message has a number that says what happened, plus two extra values (`wParam` and `lParam`) that carry details, like where the mouse was. We will use both soon.

## Breaking it up

### The window class

Before you can make a window, you have to describe what kind of window it is. That description is a **window class**. It has nothing to do with C++ classes. It's more like a template that many windows can share. We fill in a `WNDCLASSEXW` [lines 34 to 46] and register it.

```text
33      // Initialize the struct to zero
34      ZeroMemory(&wcx, sizeof(wcx));
35      wcx.cbSize = sizeof(WNDCLASSEXW);               // Must always be sizeof(WNDCLASSEXW)
36      wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // Class styles
37      wcx.lpfnWndProc = MainWndProc;                  // Pointer to the callback procedure
38      wcx.cbClsExtra = 0;                             // Extra bytes to allocate after the class
39      wcx.cbWndExtra = 0;                             // Extra bytes to allocate after each window
40      wcx.hInstance = hInstance;                      // Instance of the application
41      wcx.hIcon = NULL;                               // Class icon
42      wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);     // Class cursor
43      wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Background brush
44      wcx.lpszMenuName = NULL;                        // Menu resource
45      wcx.lpszClassName = L"DrawLite";                // Name of this class
46      wcx.hIconSm = NULL;                             // Small icon for this class
47
48      // Register this window class with Windows
49      if (!RegisterClassExW(&wcx))
50          return 1;
```

The fields we care about most are these.

- `lpfnWndProc` is a pointer to the function Windows calls with every message. This is the cook in our restaurant.
- `hCursor` is the mouse pointer shown over the window. `LoadCursorW` with `IDC_ARROW` gives us the standard arrow.
- `hbrBackground` is the brush used to paint the window's background.
- `lpszClassName` is the name we'll use to refer to this class when we create a window.

The other fields are either unused for now (`NULL` or 0) or something we'll visit later. `cbSize` must always be set to the size of the structure, or registration fails. `ZeroMemory` clears the whole structure first so nothing has stray garbage in it.

A word about `hbrBackground` [line 43]. Windows wants a brush handle here, but it also accepts a system colour number plus one, cast to a brush. That is what `(HBRUSH)(COLOR_WINDOW + 1)` is. The `+ 1` matters. If you read older tutorials you will see `(HBRUSH)COLOR_WINDOW` without it. Windows then reads the value as colour number minus one, so you end up with the menu colour instead of the window colour. Ours has the `+ 1`, so we get the right one.

### RegisterClassExW

```c
ATOM RegisterClassExW(const WNDCLASSEXW *lpwcx);  // The class description we just filled in
```

This hands our class over to Windows. It returns zero if it fails, and we quit [line 49 and 50]. There's no point carrying on without a class.

### CreateWindowExW

Now we can make the window itself.

```text
52      // Create the window
53      hwndMain = CreateWindowExW(
54          0,                                  // Extended window style
55          L"DrawLite",                        // Window class name
56          L"Chapter 2 - A window of your own",// Window title
57          WS_OVERLAPPEDWINDOW,                // Window style
58          CW_USEDEFAULT, CW_USEDEFAULT,       // (x,y) position of the window
59          CW_USEDEFAULT, CW_USEDEFAULT,       // Width and height of the window
60          NULL,                               // Parent window
61          NULL,                               // Menu
62          hInstance,                          // Application instance
63          &state);                            // Pointer passed to WM_NCCREATE and WM_CREATE
64
65      // Check if window creation was successful
66      if (!hwndMain)
67          return 1;
```

```c
HWND CreateWindowExW(DWORD dwExStyle,       // Extra style bits. 0 for a plain window
                     LPCWSTR lpClassName,   // Which class to make a window from
                     LPCWSTR lpWindowName,  // The text in the title bar
                     DWORD dwStyle,         // Style bits (border, title bar, buttons...)
                     int x, int y,          // Where to put it. CW_USEDEFAULT lets Windows choose
                     int nWidth,            // How wide
                     int nHeight,           // How tall
                     HWND hWndParent,       // The parent window. NULL for a top level window
                     HMENU hMenu,           // A menu. NULL for none (for now)
                     HINSTANCE hInstance,   // The program that owns the window
                     LPVOID lpParam);       // Anything you want to hand to the window as it is created
```

`WS_OVERLAPPEDWINDOW` is the ordinary window style. It's a combination of a title bar, a border you can resize, a system menu, and the minimise and maximise buttons.

The return value is a `HWND`, a handle to the window (remember handles from chapter 1?). We check it isn't `NULL`, because if it is, the window wasn't made.

### ShowWindow and UpdateWindow

A new window is invisible until you ask for it to be shown [line 70]. We pass along `nCmdShow`, the value Windows gave `wWinMain`, so that a shortcut set to "start minimised" works. `UpdateWindow` [line 71] makes the window paint itself right away.

### The message loop

This is the heart of every Windows program.

```text
73      // Process messages coming to this window
74      while (GetMessageW(&msg, NULL, 0, 0) > 0)
75      {
76          TranslateMessage(&msg);
77          DispatchMessageW(&msg);
78      }
79
80      // Return value to the system
81      return (int)msg.wParam;
```

`GetMessageW` takes the next message off our queue and fills in a `MSG` structure. If the queue is empty, it waits, using no CPU at all. It returns a positive number for an ordinary message, zero when it receives `WM_QUIT`, and minus one on an error. That is why we test `> 0`.

`TranslateMessage` turns key presses into character messages. We don't need it yet but it costs nothing and we'll want it later.

`DispatchMessageW` is the waiter walking over to the cook. It looks at which window the message is for, finds that window's class, and calls the function we registered as `lpfnWndProc`.

When `WM_QUIT` arrives, `GetMessageW` returns zero, the loop ends and so does the program. The exit code travels inside the message, in `wParam` [line 81].

### The window procedure

Every message for our window ends up in `MainWndProc` [line 84]. It is a `switch` on the message number.

```c
LRESULT CALLBACK WindowProc(HWND hwnd,      // The window the message is for
                            UINT msg,       // What happened, e.g. WM_LBUTTONDOWN
                            WPARAM wParam,  // First detail. Meaning depends on the message
                            LPARAM lParam); // Second detail. Meaning depends on the message
```

Notice that `MainWndProc` is `static`. Only our file needs to see it, because Windows gets hold of it through the pointer in the class.

Our function handles three messages and passes everything else to `DefWindowProcW` [line 115]. That's the head chef. It does all the boring work, like drawing the title bar, moving the window when you drag it, and resizing it. Don't forget that line. Without it the window will be there but you won't be able to do anything with it.

### WM_DESTROY and PostQuitMessage

When you close the window, Windows sends `WM_DESTROY`. The window is going away. We answer with `PostQuitMessage(0)` [line 111], which drops a `WM_QUIT` in the queue. That's the one that makes `GetMessageW` return zero and ends the loop. If you forget it, the window disappears but the program keeps running, invisible, forever. (Open Task Manager if you ever see that happen.)

### WM_LBUTTONDOWN

This is the message we wanted. It arrives when the left mouse button is pressed inside the window. The position is packed into `lParam`, and two macros from `windowsx.h` take it apart.

```text
100      case WM_LBUTTONDOWN:
101          {
102              wchar_t str[100];
103              state->clicks++;
104              StringCchPrintfW(str, 100, L"Co-ordinates are\nX=%d and Y=%d\n\nClick number %d",
105                  GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), state->clicks);
106              MessageBoxW(hwnd, str, L"Left Button Clicked", MB_OK);
107          }
108          break;
```

`GET_X_LPARAM` and `GET_Y_LPARAM` [line 105] give the x and y position in pixels, measured from the top left corner of the window's client area (the part inside the borders). `StringCchPrintfW` is a safe `swprintf`. It never writes more than the 100 characters we told it about. Then it's just `MessageBoxW` from chapter 1, with `hwnd` as the owner this time.

### Keeping state without globals

Look at the `clicks` counter [line 103]. In a lot of old Win32 code, it'd be a global variable. We keep it in a struct instead [lines 13 to 16] and tie that struct to the window. The trick takes three steps.

1. We pass `&state` as the last argument of `CreateWindowExW` [line 63].
2. In `WM_NCCREATE`, the very first message a window gets, that pointer arrives inside a `CREATESTRUCTW`. We store it in the window with `SetWindowLongPtrW` and `GWLP_USERDATA` [lines 96 and 97].
3. At the top of `MainWndProc`, we fetch it back with `GetWindowLongPtrW` [line 88].

`GWLP_USERDATA` is a spare slot, one pointer wide, that Windows keeps for every window and does nothing with. It's there for exactly this. One thing to watch is that a few messages arrive before `WM_NCCREATE`, so the pointer can be `NULL` until we've stored it. That is why the comment says so [line 87]. Our click handler is safe because clicks come much later.

Don't worry if this is hazy. We'll use it in every chapter from now on, and in chapter 4 it moves into a proper per-window struct.

## Adding functionality

Let's make the right button do something too. Add this case to the `switch` in `MainWndProc`, just before `WM_DESTROY` [line 109].

```c
    case WM_RBUTTONDOWN:
        MessageBoxW(hwnd, L"You pressed the right button.", L"Right Button", MB_OK);
        break;
```

Rebuild and try it. Its as simple as that! Every kind of input is just another message with another name.

## Exercise

Show the number of clicks in the window's title bar, instead of in a message box.

*Hint: `SetWindowTextW` changes a window's title, and `StringCchPrintfW` can build the string.*

## That's it

You have a window, a message loop and a click handler. That's the skeleton every Win32 program is built on. Next we give the window a menu, an icon, a dialog and a manifest.

[Chapter 3: Resources](../03-resources/README.md)
