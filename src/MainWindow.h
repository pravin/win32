/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (March 08, 2007)
 * http://www.dustyant.com/win32
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>

// Class: MainWindow
// Draws the Main Application Window
class MainWindow
{
public:
    MainWindow(HINSTANCE hInstance);
    ~MainWindow();
    static LRESULT CALLBACK MainWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void OnCommand(HWND hwnd, int id, HWND hCtl, UINT codeNotify);
    bool Run(int nCmdShow);

private:
    WNDCLASSEX m_wndClass;
    static HINSTANCE m_hInstance;
    HWND m_hwnd;
    static char m_szClassName[];
};
