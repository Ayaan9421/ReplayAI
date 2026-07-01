#include "HotkeyListener.h"

#include <iostream>

using namespace std;

HotkeyListener::HotkeyListener() = default;

HotkeyListener::~HotkeyListener() { UnregisterHotKey(nullptr, HOTKEY_ID); }

bool HotkeyListener::initialize() {
    // Ctrl + Shift + F9
    if (!RegisterHotKey(nullptr, HOTKEY_ID, MOD_CONTROL | MOD_SHIFT, VK_F9)) {
        cout << "[Hotkey] Failed to register hotkey\n";
        return false;
    }

    cout << "[Hotkey] Registered Ctrl + Shift + F9\n";
    return true;
}

bool HotkeyListener::poll() {
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_ID) { return true; }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return false;
}
