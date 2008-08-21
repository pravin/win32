/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (March 08, 2007)
 * http://www.dustyant.com/win32
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>
#include "MainWindow.h"

int WINAPI
    WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;

    MainWindow *winMain = new MainWindow(hInst);
    if(!winMain->Run(nCmdShow))
    {
        delete winMain;
        return 1; // error
    }

    // Run the message loop. It will run until GetMessage() returns 0
    while (GetMessage (&msg, NULL, 0, 0))
    {
        // Translate virtual-key messages into character messages
        TranslateMessage(&msg);
        // Send message to WindowProcedure
        DispatchMessage(&msg);
    }

    delete winMain;

    return msg.wParam;
}
