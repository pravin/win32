# Chapter 3 - Resources

[< Chapter 2: A window of your own](../02-a-window-of-your-own/README.md)

Our window works, but it looks like a prototype. It has no menu, a generic icon and nothing to click. In this chapter we fix all of that, using resources.

In this lesson you will

- give the program an icon, a menu and keyboard shortcuts,
- show an About box from a dialog resource,
- add version information to the exe,
- learn what a manifest is and why every modern Win32 program needs one.

## Before we begin

This chapter adds a `res` folder and one header, `src/resource.h`. `src/main.c` and `CMakeLists.txt` changed too. Build it as before (see [chapter 2](../02-a-window-of-your-own/README.md) if you've forgotten how), and run it.

```text
cmake -S . -B build && cmake --build build
```

![The DrawLite window with its menu and About box](images/screenshot.png)

You get a window with a File menu and a Help menu. Try `Ctrl+N`, `Ctrl+O` and `Ctrl+S`. Open Help, About DrawLite. Then find `drawlite.exe` in Explorer, right click it, choose Properties and have a look at the Details tab. All of that came from resources.

Something else you'll notice is that the click counter from chapter 2 is gone. We took it out to keep this chapter small. The per-window state trick is back in the next chapter.

## What is a resource?

A resource is a chunk of data that gets glued onto the end of your exe at build time. Icons, menus, dialogs, keyboard shortcut tables, version info and the manifest all work this way. You describe them in a plain text **resource script**, a `.rc` file, and a tool called the resource compiler turns it into something the linker can add to the program. CMake handles that for us once we list the `.rc` file with our sources and say `project(DrawLite C RC)` [lines 2 and 8 to 11 of `CMakeLists.txt`].

Why not just write menus in C? You can, with calls like `CreateMenu`. But a resource keeps the layout out of your code. You can change a caption or move a button without touching a single `.c` file. And Windows can load things like icons for you, before your program has run a line of code. That's how Explorer gets your icon.

## The identifiers

The C code and the `.rc` file both need to agree on what to call things. We keep the names in one header that both can include.

```text
 8  #define IDI_APP         1
 9
10  #define IDD_ABOUT       100
11  #define IDC_STATIC      -1
12
13  #define IDM_MAINMENU    200
14  #define IDA_MAINACCEL   201
15
16  #define IDM_FILE_NEW    301
17  #define IDM_FILE_OPEN   302
18  #define IDM_FILE_SAVE   303
19  #define IDM_FILE_EXIT   304
20  #define IDM_HELP_ABOUT  305
21
22  #endif
```

Each name is just a number. Menu items are 301 and up. The About box is 100, and so on. Windows doesn't care what the numbers are as long as they're unique within their kind. `IDC_STATIC` is -1, which means "I don't need to talk to this control".

## The resource script

Here is the whole `.rc` file, in pieces.

### Icon

```text
7  // The application icon. Windows uses the lowest numbered icon as the exe's icon.
8  IDI_APP ICON "drawlite.ico"
```

The first word is the id from `resource.h`, then the kind of resource, then the file. Windows uses the lowest numbered icon as the icon Explorer shows for the exe, and ours is number 1. If you want your own, replace `drawlite.ico`. A good icon file holds several sizes (16, 32, 48 and 256 pixels) and Windows picks the best one.

### Menu

```text
13  // Main menu
14  IDM_MAINMENU MENU
15  BEGIN
16      POPUP "&File"
17      BEGIN
18          MENUITEM "&New\tCtrl+N",    IDM_FILE_NEW
19          MENUITEM "&Open...\tCtrl+O", IDM_FILE_OPEN
20          MENUITEM "&Save\tCtrl+S",   IDM_FILE_SAVE
21          MENUITEM SEPARATOR
22          MENUITEM "E&xit",           IDM_FILE_EXIT
23      END
24      POPUP "&Help"
25      BEGIN
26          MENUITEM "&About DrawLite", IDM_HELP_ABOUT
27      END
28  END
```

`&` puts an underline under the next letter, so `&File` gives you `Alt+F`. The `\t` is a tab. It lines up the shortcut text in a column, as you've seen in other programs. Beware, though, because writing `Ctrl+N` in the menu doesn't make `Ctrl+N` work. That's the accelerator table's job.

### Accelerators

```text
30  // Keyboard shortcuts
31  IDA_MAINACCEL ACCELERATORS
32  BEGIN
33      "N", IDM_FILE_NEW,  VIRTKEY, CONTROL
34      "O", IDM_FILE_OPEN, VIRTKEY, CONTROL
35      "S", IDM_FILE_SAVE, VIRTKEY, CONTROL
36  END
```

Each line is a key, the command it should send, and some flags. `VIRTKEY` says the key is a virtual key code (the `N` key, not the character n), and `CONTROL` says Ctrl must be held down.

### Dialog

```text
38  // About box. Sizes are in dialog units, not pixels, so it scales with the font.
39  IDD_ABOUT DIALOGEX 0, 0, 180, 100
40  STYLE DS_MODALFRAME | DS_SHELLFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
41  CAPTION "About DrawLite"
42  FONT 9, "Segoe UI"
43  BEGIN
44      ICON            IDI_APP, IDC_STATIC, 74, 12, 32, 32
45      CTEXT           "DrawLite v0.3", IDC_STATIC, 20, 52, 140, 8
46      CTEXT           "A drawing application for Windows", IDC_STATIC, 7, 64, 166, 8
47      DEFPUSHBUTTON   "OK", IDOK, 65, 80, 50, 14
48  END
```

A dialog is a little window with controls on it, laid out in the resource script. The numbers are in **dialog units**, not pixels. A dialog unit is based on the size of the dialog's font, so the box scales sensibly when the text gets bigger. `DS_SHELLFONT` with `FONT 9, "Segoe UI"` asks for the font that other Windows dialogs use. `DS_MODALFRAME` and `DS_CENTER` give us the usual boxed look, centred on the screen.

The controls are an `ICON`, two lines of centred text, and a default push button. `IDOK` is a predefined id. Pressing Enter in a dialog presses the default button.

### Version information

```text
50  // Version information. Right click the exe, Properties, Details to see it.
51  VS_VERSION_INFO VERSIONINFO
52  FILEVERSION     0,3,0,0
53  PRODUCTVERSION  0,3,0,0
54  FILEFLAGSMASK   VS_FFI_FILEFLAGSMASK
55  FILEFLAGS       0
56  FILEOS          VOS_NT_WINDOWS32
57  FILETYPE        VFT_APP
58  FILESUBTYPE     0
59  BEGIN
60      BLOCK "StringFileInfo"
61      BEGIN
62          BLOCK "040904B0"
63          BEGIN
64              VALUE "CompanyName",      "Pravin Paratey"
65              VALUE "FileDescription",  "DrawLite - a drawing application"
66              VALUE "FileVersion",      "0.3.0.0"
67              VALUE "InternalName",     "DrawLite"
68              VALUE "LegalCopyright",   "Copyright (c) Pravin Paratey"
69              VALUE "OriginalFilename", "drawlite.exe"
70              VALUE "ProductName",      "DrawLite"
71              VALUE "ProductVersion",   "0.3.0.0"
72          END
73      END
74      BLOCK "VarFileInfo"
75      BEGIN
76          VALUE "Translation", 0x0409, 1200
77      END
78  END
```

You can ignore most of this. It's boilerplate, and every Windows program has a block just like it. The bits that matter are the version numbers and the strings in `StringFileInfo`, which are what the Details tab shows. `040904B0` is the language (US English) and the code page (Unicode). Update the version here when you release a new one.

## Loading resources from C

Now the code. Everything starts in `wWinMain`.

### Icons and the menu

```text
35      // Icons and the menu come from the resources linked into our exe
36      wcx.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
37      wcx.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
38          GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
39      wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);
40      wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
41      wcx.lpszMenuName = MAKEINTRESOURCEW(IDM_MAINMENU);
42      wcx.lpszClassName = L"DrawLite";
```

```c
HICON LoadIconW(HINSTANCE hInstance,    // The module that owns the resource. Our own exe
                LPCWSTR lpIconName);    // The id, wrapped by MAKEINTRESOURCEW
```

`MAKEINTRESOURCEW(IDI_APP)` [line 36] is a macro that turns a number into the shape these functions expect. Windows reuses one string parameter to mean "a name" or "a number", and a small number cast to a pointer means the latter.

`LoadIconW` gives us the icon at the system's default large size. For the small icon in the title bar [lines 37 and 38] we use `LoadImageW`, which lets us say exactly what size we want.

```c
HANDLE LoadImageW(HINSTANCE hInst,  // The module that owns the resource
                  LPCWSTR name,     // The id, wrapped by MAKEINTRESOURCEW
                  UINT type,        // IMAGE_ICON, IMAGE_BITMAP or IMAGE_CURSOR
                  int cx, int cy,   // The size you want. We ask Windows for its small icon size
                  UINT fuLoad);     // Flags. 0 is fine for us
```

`GetSystemMetrics` with `SM_CXSMICON` and `SM_CYSMICON` tells us how big a small icon should be on this machine. The menu is attached to the whole window class by one line, `lpszMenuName` [line 41]. Every window we make from that class gets the menu.

### Accelerators and the message loop

```text
56      // Load the keyboard shortcuts
57      hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDA_MAINACCEL));
58
59      while (GetMessageW(&msg, NULL, 0, 0) > 0)
60      {
61          // Give the accelerator table first go at the message.
62          // If it was a shortcut, it turns into a WM_COMMAND and we are done.
63          if (!TranslateAcceleratorW(hwndMain, hAccel, &msg))
64          {
65              TranslateMessage(&msg);
66              DispatchMessageW(&msg);
67          }
68      }
```

We load the table once, then change the message loop a little. Before a message goes to `TranslateMessage`, we offer it to `TranslateAcceleratorW` [line 63]. If the message was a key press that matches a shortcut, the function turns it into a `WM_COMMAND` for our window and returns nonzero. In that case we mustn't dispatch the original message, so we skip it. Otherwise it's business as usual.

```c
int TranslateAcceleratorW(HWND hWnd,        // The window that should get the WM_COMMAND
                          HACCEL hAccTable, // The table we loaded
                          LPMSG lpMsg);     // The message we just got from GetMessageW
```

### WM_COMMAND

Menu clicks and accelerators arrive the same way, as `WM_COMMAND`. The id of the item is in the low word of `wParam`.

```text
78      case WM_COMMAND:
79          // Menu items and accelerators both land here. The id is in the low word of wParam.
80          switch (LOWORD(wParam))
81          {
82          case IDM_FILE_NEW:
83          case IDM_FILE_OPEN:
84          case IDM_FILE_SAVE:
85              MessageBoxW(hwnd, L"Not written yet. Patience!", L"DrawLite", MB_OK | MB_ICONINFORMATION);
86              break;
87          case IDM_FILE_EXIT:
88              // Ask the window to close. This also sends WM_DESTROY.
89              DestroyWindow(hwnd);
90              break;
91          case IDM_HELP_ABOUT:
92              DialogBoxParamW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUT), hwnd, AboutDlgProc, 0);
93              break;
94          }
95          return 0;
```

That's why the shortcut and the menu item can share one id. The program can't tell them apart, and doesn't have to. `File > New`, `Open` and `Save` just say "Not written yet. Patience!" for now. `Exit` calls `DestroyWindow` [line 89], which sends us the `WM_DESTROY` we already handle.

### Showing the About box

```text
103  // Function: AboutDlgProc
104  // Handles messages for the About box. Dialog procs return TRUE if they
105  // handled a message and FALSE if they did not.
106  static INT_PTR CALLBACK
107  AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
108  {
109      UNREFERENCED_PARAMETER(lParam);
110
111      switch (msg)
112      {
113      case WM_INITDIALOG:
114          return TRUE;
115      case WM_COMMAND:
116          if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
117          {
118              EndDialog(hDlg, IDOK);
119              return TRUE;
120          }
121          break;
122      }
123      return FALSE;
124  }
```

```c
INT_PTR DialogBoxParamW(HINSTANCE hInstance,     // The module that owns the dialog resource
                        LPCWSTR lpTemplateName,  // The dialog's id, wrapped by MAKEINTRESOURCEW
                        HWND hWndParent,         // The window that owns the dialog
                        DLGPROC lpDialogFunc,    // Our function that handles its messages
                        LPARAM dwInitParam);     // A value passed to WM_INITDIALOG. We don't need one
```

The call is on [line 92]. `DialogBoxParamW` is a **modal** dialog. It doesn't return until the dialog is closed, and while it's open the main window can't be used. Like `MessageBoxW` in chapter 1, it runs a message loop of its own while it waits.

A dialog procedure is like a window procedure, with two differences. It returns `TRUE` if it handled the message and `FALSE` if it didn't, and it never calls `DefWindowProcW`. Windows does the default work itself. We handle `WM_INITDIALOG`, which is the dialog's `WM_CREATE`, and `WM_COMMAND` for the OK button. `EndDialog` [line 118] closes the box. We test for `IDCANCEL` too so the Escape key and the close button work.

Notice that we needed `g_hInstance` [line 15] to make that call from inside `MainWndProc`. A global is the easy way, so that's what we've used this time. Chapter 4 does it more tidily.

## The manifest

Now for the odd one out. Look at the third item in the `.rc` file.

```text
10  // The manifest. Resource type 24 is RT_MANIFEST, and 1 is its id.
11  1 24 "app.manifest"
```

Resource type 24 is `RT_MANIFEST`. A manifest is a small XML file that tells Windows things about your program before it runs. It's a bit like a note pinned to the front of a parcel. Without one, Windows assumes your program was written years ago and treats it that way. You get the old grey look for buttons, and the screen is blurry on a high resolution display.

Here is ours, `res/app.manifest`.

```text
 1  <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
 2  <assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
 3    <assemblyIdentity type="win32" name="DrawLite.App" version="1.0.0.0" />
 4    <description>DrawLite</description>
 5
 6    <!-- Use version 6 of the common controls (modern look) -->
 7    <dependency>
 8      <dependentAssembly>
 9        <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls"
10          version="6.0.0.0" processorArchitecture="*"
11          publicKeyToken="6595b64144ccf1df" language="*" />
12      </dependentAssembly>
13    </dependency>
14
15    <!-- We run on Windows 10 and 11 -->
16    <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
17      <application>
18        <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}" />
19      </application>
20    </compatibility>
21
22    <!-- Tell Windows we handle high DPI screens ourselves -->
23    <application xmlns="urn:schemas-microsoft-com:asm.v3">
24      <windowsSettings>
25        <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
26        <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
27      </windowsSettings>
28    </application>
29  </assembly>
```

It says three things.

**Common controls version 6.** The `dependency` block asks for version 6 of the common controls library, `comctl32`. Without it, buttons, toolbars and the like get the flat Windows 95 style. With it they get the current look. Chapter 5 uses these controls.

**Windows 10 and 11.** The `supportedOS` id is the one Microsoft assigned to Windows 10 and later. Leave it in, and Windows won't apply the compatibility tricks it uses for older programs.

**Per-monitor DPI awareness.** People run Windows at 125, 150 or 200 percent scaling, and different monitors can have different settings. If your program doesn't say it can cope, Windows draws it at 100 percent and stretches the picture, which looks fuzzy. `PerMonitorV2` says "I'll handle it. Tell me when the scale changes". The older `true/pm` line is there for versions of Windows that don't understand the newer setting. Our code doesn't do any scaling work yet. The dialog scales on its own, because it's in dialog units, and we'll tackle our own drawing later on.

### A word about MSVC

If you build with Visual Studio, the linker will happily make a manifest for you, and then complain that it found two, because ours is already inside the `.rc` file. That's why `CMakeLists.txt` has this.

```text
23  if(MSVC)
24      target_compile_options(drawlite PRIVATE /W4 /utf-8)
25      # Our manifest is inside the .rc file, so stop the linker making its own
26      target_link_options(drawlite PRIVATE /MANIFEST:NO)
```

`/MANIFEST:NO` tells the linker to leave the manifest alone. MinGW doesn't make one, so it doesn't need the flag. I could only test with MinGW (and Wine), so if MSVC gives you trouble here, please tell me.

## Adding functionality

Add a Help menu entry that shows a message instead of the About box. In `drawlite.rc`, put a line in the Help popup

```text
MENUITEM "&Tips",   IDM_HELP_TIPS
```

Add `#define IDM_HELP_TIPS 306` to `resource.h`. Then add a `case IDM_HELP_TIPS:` to the `switch` in `MainWndProc`, just above `IDM_HELP_ABOUT` [line 91], and show a `MessageBoxW`. Rebuild. Notice that you didn't write a line of C to make the menu item appear.

## Exercise

Add a shortcut for the About box. Pick `F1`.

*Hint: In the accelerator table, `VK_F1` goes in place of "N", and you don't need `CONTROL`. Write `VIRTKEY` and nothing else.*

## That's it

Whew. That was a lot of files for one small window! But icons, menus, dialogs and a manifest are in nearly every Windows program you've ever used, and you now know where each one comes from. Next we tidy up the code before it gets any bigger.

[Chapter 4: Housekeeping](../04-housekeeping/README.md)
