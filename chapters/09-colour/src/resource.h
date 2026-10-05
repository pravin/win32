/* DrawLite - Win32 Tutorial
 * Chapter 9 - Colour
 * Resource identifiers shared by the C code and the .rc file.
 */
#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_APP         1

#define IDD_ABOUT       100
#define IDD_NEWIMAGE    101
#define IDC_STATIC      -1
#define IDC_NEW_WIDTH   110
#define IDC_NEW_HEIGHT  111

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
#define IDC_CANVAS      403
#define IDC_SWAP        404
#define IDC_ALPHA       405
#define IDC_ALPHA_LABEL 406

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

// Options menu
#define IDM_BRUSH_FIRST     600
#define IDM_BRUSH_1         600
#define IDM_BRUSH_2         601
#define IDM_BRUSH_4         602
#define IDM_BRUSH_8         603
#define IDM_BRUSH_16        604
#define IDM_BRUSH_32        605
#define IDM_BRUSH_LAST      605
#define IDM_OPT_SOFT        610

#endif
