#ifndef CHILDWINDOW_H_INCLUDED
#define CHILDWINDOW_H_INCLUDED

class ChildWindow {
public:
    ChildWindow();
    ~ChildWindow();

    static LRESULT CALLBACK MainWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void OnCommand(HWND hwnd, int id, HWND hCtl, UINT codeNotify);
private:
    WNDCLASSEX m_wndClass;
    static HINSTANCE m_hInstance;
    HWND m_hwnd;
    static char m_szClassName[];
};

#endif // CHILDWINDOW_H_INCLUDED
