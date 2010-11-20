/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (March 08, 2007)
 * http://www.dustyant.com/wintut
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>

int WINAPI
    WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
    MessageBox(NULL, "Hello World! This is my first win32 program!",
        "Lesson1", MB_OK);

    return 0;
}
