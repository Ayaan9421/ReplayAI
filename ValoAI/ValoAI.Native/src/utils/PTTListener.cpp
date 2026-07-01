#include "PTTListener.h"
#include <iostream>

PTTListener* PTTListener::s_instance = nullptr;

bool PTTListener::start(Callback onPTTChange) {
    s_instance = this;
    m_callback = onPTTChange;

    // WH_KEYBOARD_LL is a passive hook — key still reaches the game
    m_hook = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, nullptr, 0);
    if (!m_hook) {
        std::cout << "[PTTListener] Failed to install hook\n";
        return false;
    }
    std::cout << "[PTTListener] Monitoring T and U for PTT\n";
    return true;
}

void PTTListener::stop() {
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
}

LRESULT CALLBACK PTTListener::hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (kb->vkCode == 0x54 || kb->vkCode == 0x55) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                // LLKHF_UP is 0x80 in flags — if not set, it's a down event
                // But low-level hooks don't send repeats like WM_KEYDOWN does
                // The real issue: track per-key state instead of a counter
                bool wasActive = s_instance->m_activeKeys.load() > 0;
                s_instance->m_activeKeys |= (kb->vkCode == 0x54 ? 1 : 2);
                if (!wasActive && s_instance->m_callback) {
                    s_instance->m_callback(true);
                }
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                s_instance->m_activeKeys &= ~(kb->vkCode == 0x54 ? 1 : 2);
                if (s_instance->m_activeKeys.load() == 0 && s_instance->m_callback) {
                    s_instance->m_callback(false);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}