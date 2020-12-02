#include <windows.h>

bool global_running;

static LRESULT CALLBACK
main_win_cb (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    LRESULT ret = {};
    switch (uMsg) {
        case WM_PAINT: {
            ret = 0;
        } break;
        case WM_CLOSE: {
            global_running = false;
            DestroyWindow(hwnd);
            ret = 0;
        } break;
        default: {
            ret = DefWindowProcA(hwnd, uMsg, wParam, lParam);
        } break;
    }
    return ret;
}

INT WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, INT) {
    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.lpfnWndProc   = main_win_cb;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "d3d12_win32";

    if (RegisterClassA(&wc)) {

        HWND hwnd = CreateWindowExA(
            0,                                      // Optional window styles.
            wc.lpszClassName,                       // Window class
            "Learn D3D12 win32 application",        // Window text
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,       // Window style
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, // Size and position settings
            0 /* Parent window */,  0 /* Menu */, hInstance /* Instance handle */, 0 /* Additional application data */
        );

        if (hwnd) {
            global_running = true;
            while(global_running) {
                MSG msg = {};
                while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                }
            }
        }
    }

    return 0;
}
