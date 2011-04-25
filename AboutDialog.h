/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (April 22, 2007)
 * http://pravin.insanitybegins.com/win32
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>
#include "resource.h"

// Class: AboutDialog
// Draws the About Dialog
class AboutDialog
{
public:
    AboutDialog();
    ~AboutDialog();
    static BOOL CALLBACK DialogProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    int Run(HINSTANCE hInstance, HWND hParent);

private:
    HWND m_hwnd;
};
