#include <windows.h>

#include <iostream>

#define HOTKEY_TRIGGER 1
#define HOTKEY_EXIT 2

using namespace std;
int main() {
    cout << "Registering hotkey CTRL + SHIFT + F10...." << endl;
    if (!RegisterHotKey(nullptr, HOTKEY_TRIGGER, MOD_CONTROL | MOD_SHIFT, VK_F10)) {
        cerr << "Failed to register hotkey" << endl;
        return -1;
    }

    if (!RegisterHotKey(nullptr, HOTKEY_EXIT, MOD_CONTROL | MOD_SHIFT, 'Q')) {
        cerr << "Failed to register hotkey" << endl;
        return -1;
    }

    cout << "hot key registered" << endl;
    cout << " press ctrl + shift +f10 " << endl;
    cout << " press esc to exit" << endl;

    MSG msg{};
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                if (msg.wParam == HOTKEY_TRIGGER) { cout << "hotkey pressed" << endl; }
            }

            if (msg.message == WM_HOTKEY) {
                if (msg.wParam == HOTKEY_EXIT) {
                    cout << "exit pressed" << endl;
                    goto exit;
                }
            }
        }
        Sleep(5);
    }
exit:
    UnregisterHotKey(nullptr, HOTKEY_TRIGGER);
    UnregisterHotKey(nullptr, HOTKEY_EXIT);

    return 0;
}