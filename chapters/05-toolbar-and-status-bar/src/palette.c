/* DrawLite - Win32 Tutorial
 * Chapter 5 - Toolbar and status bar
 *
 * File: palette.c
 */

#include <windows.h>
#include "palette.h"
#include "resource.h"
#include "uiutil.h"

#define PALETTE_CLASS L"DrawLitePalette"

static LRESULT CALLBACK Palette_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void Palette_Layout(HWND hwnd);

static BOOL
Palette_RegisterClass(HINSTANCE hInstance)
{
    static BOOL registered = FALSE;
    WNDCLASSEXW wcx;

    if (registered)
        return TRUE;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize = sizeof(wcx);
    wcx.lpfnWndProc = Palette_WndProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcx.lpszClassName = PALETTE_CLASS;

    registered = RegisterClassExW(&wcx) != 0;
    return registered;
}

HWND
Palette_Create(HWND parent, HINSTANCE hInstance, int id)
{
    HWND hwnd;
    int i;

    if (!Palette_RegisterClass(hInstance))
        return NULL;

    hwnd = CreateWindowExW(0, PALETTE_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, hInstance, NULL);
    if (!hwnd)
        return NULL;

    // One push-like radio button per tool. Radio buttons give us
    // "only one pressed at a time" for free.
    for (i = 0; i < TOOL_COUNT; i++)
    {
        DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE;
        if (i == 0)
            style |= WS_GROUP | WS_TABSTOP;
        CreateWindowExW(0, L"BUTTON", Tool_Name((ToolId)i), style, 0, 0, 0, 0,
            hwnd, (HMENU)(INT_PTR)(IDM_TOOL_FIRST + i), hInstance, NULL);
    }
    return hwnd;
}

void
Palette_SetTool(HWND palette, ToolId tool)
{
    // Pressing one of the group's buttons un-presses the others for us
    CheckRadioButton(palette, IDM_TOOL_FIRST, IDM_TOOL_FIRST + TOOL_COUNT - 1, IDM_TOOL_FIRST + tool);
}

// Function: Palette_Layout
// Lays the tool buttons out in two columns, filling each row left to right.
static void
Palette_Layout(HWND hwnd)
{
    RECT rc;
    UINT dpi = GetDpiForWindow(hwnd);
    int margin = Ui_Scale(dpi, 6);
    int height = Ui_Scale(dpi, 28);
    int width, i;

    GetClientRect(hwnd, &rc);
    width = (rc.right - 3 * margin) / 2; // two buttons and three gaps across
    for (i = 0; i < TOOL_COUNT; i++)
    {
        HWND button = GetDlgItem(hwnd, IDM_TOOL_FIRST + i);
        int col = i % 2;
        int row = i / 2;
        MoveWindow(button, margin + col * (width + margin), margin + row * (height + margin / 2),
            width, height, TRUE);
    }
}

static LRESULT CALLBACK
Palette_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        Palette_Layout(hwnd);
        return 0;
    case WM_COMMAND:
        // Buttons tell their parent, which is us. We pass it up to the main window.
        SendMessageW(GetParent(hwnd), WM_COMMAND, wParam, lParam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
