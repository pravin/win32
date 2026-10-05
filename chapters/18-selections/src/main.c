/* DrawLite - Win32 Tutorial
 * Chapter 18 - Selections
 * by Pravin Paratey (December 2009, rewritten 2026)
 *
 * Source released under the MIT licence. See LICENSE in the root folder.
 */

#include <windows.h>
#include "mainwindow.h"
#include "imageio.h"
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

    // Starts COM and Windows Imaging Component, for PNG and JPEG
    if (!ImageIO_Init())
    {
        MessageBoxW(NULL, L"Windows Imaging Component is not available.", L"DrawLite", MB_OK | MB_ICONERROR);
        return 1;
    }

    mainWindow = MainWindow_Create(hInstance, nCmdShow, lpCmdLine);
    if (!mainWindow)
    {
        ImageIO_Shutdown();
        return 1;
    }

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
    ImageIO_Shutdown();
    return (int)msg.wParam;
}
