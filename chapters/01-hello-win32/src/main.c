/* DrawLite - Win32 Tutorial
 * Chapter 1 - Hello, Win32
 * by Pravin Paratey (October 2002, rewritten 2026)
 *
 * Source released under the MIT licence. See LICENSE in the root folder.
 */

#include <windows.h>

int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    MessageBoxW(NULL, L"Hello World! This is my first WIN32 program",
        L"Chapter 1", MB_OK);

    return 0;
}
