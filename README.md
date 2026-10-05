# DrawLite: learn Win32 programming in C

Years back I wanted to write programs for Windows and couldn't find a tutorial that started at the beginning. This is the one I wish I'd had. Over twenty chapters we build **DrawLite**, a small paint program with layers, selections, text and effects, using nothing but C and the Windows API. No MFC, no frameworks, no third party libraries.

The first chapters were written in 2002. This is the 2026 rewrite: C17, Unicode, CMake, Windows 10 and 11, high DPI, and a build that works with Visual Studio 2022, MinGW-w64 or the plain command line.

## How it works

Every chapter is a folder you can build on its own. Chapter 2 starts from a copy of chapter 1 and adds to it, chapter 3 starts from chapter 2, and so on, so the last folder is the finished program. Each folder has a `README.md` with the lesson. Read it right here on GitHub, then build the code next to it.

You need Windows 10 or 11 and some C. You don't need to know anything about Windows programming. [Chapter 1](chapters/01-hello-win32/README.md) explains how to set up a compiler.

To build a chapter (any compiler CMake can find):

```text
cd chapters/08-pencil-and-brush
cmake -S . -B build
cmake --build build --config Release
```

Visual Studio users can simply choose *Open a local folder* on a chapter folder. MinGW-w64 users should add `-G Ninja` (or `-G "MinGW Makefiles"`).

## The chapters

### Part 1: Getting started

Windows, messages, resources, and the project skeleton.

1. [Hello, Win32](chapters/01-hello-win32/README.md)
2. [A window of your own](chapters/02-a-window-of-your-own/README.md)
3. [Resources](chapters/03-resources/README.md)
4. [Housekeeping](chapters/04-housekeeping/README.md)
5. [Toolbar and status bar](chapters/05-toolbar-and-status-bar/README.md)

### Part 2: Drawing

A canvas you own, pixels you own, and the tools that draw on them.

6. [The canvas](chapters/06-the-canvas/README.md)
7. [Pixels you own](chapters/07-pixels-you-own/README.md)
8. [Pencil and brush](chapters/08-pencil-and-brush/README.md)
9. [Colour](chapters/09-colour/README.md)
10. [Shapes](chapters/10-shapes/README.md)
11. [Fill, picker and eraser](chapters/11-fill-picker-eraser/README.md)

### Part 3: A real document

Zoom, undo, and files in BMP, PNG and JPEG, plus the clipboard.

12. [Zoom and scroll](chapters/12-zoom-and-scroll/README.md)
13. [Undo and redo](chapters/13-undo-and-redo/README.md)
14. [Open and save](chapters/14-open-and-save-bmp/README.md)
15. [PNG, JPEG and the clipboard](chapters/15-png-jpeg-clipboard/README.md)

### Part 4: Layers

Layers, opacity and blend modes, and our own file format.

16. [Layers](chapters/16-layers/README.md)
17. [Layers panel and blend modes](chapters/17-layers-panel-and-blend-modes/README.md)

### Part 5: Power tools

Selections, text, and effects on a worker thread.

18. [Selections](chapters/18-selections/README.md)
19. [The text tool](chapters/19-text-tool/README.md)
20. [Effects](chapters/20-effects/README.md)

## Licence

The code is released under the MIT licence (see `LICENSE`). The text of the tutorial is by Pravin Paratey and is released under CC BY 4.0.

## Testing, honestly

Every chapter builds without warnings with MinGW-w64 and was run under Wine. The GitHub Actions workflow in `.github/workflows/build.yml` builds every chapter with both MSVC and MinGW on each push. If you find something that behaves differently on real Windows, please open an issue.
