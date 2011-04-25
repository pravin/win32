/* DrawLite - Windows Programming Tutorial
 * by Pravin Paratey (April 22, 2007)
 * http://www.dustyant.com/win32
 *
 * Source released under
 * Creative Commons Attribution-Noncommercial-No Derivative Works 3.0
 * http://creativecommons.org/licenses/by-nc-nd/3.0/
 */

#include <windows.h>
#include "AboutDialog.h"
#include "resource.h"

AboutDialog::AboutDialog()
{
}

AboutDialog::~AboutDialog()
{
}

// Function: Run
// Returns: Result of the DialogBox
int AboutDialog::Run(HINSTANCE hInstance, HWND hParent)
{
    int retval = DialogBox(
        hInstance,
        MAKEINTRESOURCE(IDD_ABOUT), // Dialog template
        hParent, // Pointer to parent hwnd
        DialogProc);
	return retval;
}

// Function: DialogProc
// Handles the messages for the About dialog
BOOL CALLBACK
    AboutDialog::DialogProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int retVal = false;
    switch(msg)
    {
    case WM_INITDIALOG:
        retVal = true;
        break;
    case WM_COMMAND:
        if(LOWORD(wParam)== IDOK)
            EndDialog(hwnd, TRUE);
        break;
    case WM_CLOSE:
        EndDialog(hwnd, TRUE);
        break;
    }
    return retVal;
}
