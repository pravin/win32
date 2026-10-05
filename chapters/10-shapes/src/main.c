/* DrawLite - Win32 Tutorial
 * Chapter 10 - Shapes
 * by Pravin Paratey (December 2009, rewritten 2026)
 *
 * Source released under the MIT licence. See LICENSE in the root folder.
 */

#include <windows.h>
#include "mainwindow.h"
#include "resource.h"

// Function: wWinMain
// The entry point of the program. It creates the main window and then
// runs the message loop until the window is closed.
//
// Returns:
//   The exit code that was passed to PostQuitMessage, or 1 if we could not start.
int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    MainWindow *mainWindow;
    HACCEL hAccel;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    mainWindow = MainWindow_Create(hInstance, nCmdShow);
    if (!mainWindow)
        return 1;

    hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDA_MAINACCEL));

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        if (!TranslateAcceleratorW(MainWindow_GetHwnd(mainWindow), hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    MainWindow_Destroy(mainWindow);
    return (int)msg.wParam;
}
