/* DrawLite - Win32 Tutorial
 * Chapter 5 - Toolbar and status bar
 * Resource identifiers shared by the C code and the .rc file.
 */
#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_APP         1

#define IDD_ABOUT       100
#define IDC_STATIC      -1

#define IDM_MAINMENU    200
#define IDA_MAINACCEL   201

#define IDM_FILE_NEW    301
#define IDM_FILE_OPEN   302
#define IDM_FILE_SAVE   303
#define IDM_FILE_EXIT   304
#define IDM_EDIT_UNDO   310
#define IDM_EDIT_REDO   311
#define IDM_HELP_ABOUT  305

// Child windows
#define IDC_TOOLBAR     400
#define IDC_STATUSBAR   401
#define IDC_PALETTE     402

// Tool buttons. The palette sends these to the main window in WM_COMMAND.
#define IDM_TOOL_FIRST      500
#define IDM_TOOL_PENCIL     500
#define IDM_TOOL_BRUSH      501
#define IDM_TOOL_ERASER     502
#define IDM_TOOL_LINE       503
#define IDM_TOOL_RECT       504
#define IDM_TOOL_ELLIPSE    505
#define IDM_TOOL_FILL       506
#define IDM_TOOL_PICKER     507

#endif
