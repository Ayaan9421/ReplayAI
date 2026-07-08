#include "PTTListener.h"
#include <iostream>
#include "utils/ValoAILog.h"

PTTListener* PTTListener::s_instance = nullptr;

bool PTTListener::start(Callback onPTTChange) {
    s_instance = this;
    m_callback = onPTTChange;
    m_running = true;

    // Run hook + message pump on a dedicated thread
    // WH_KEYBOARD_LL requires a message pump on the installing thread
    m_thread = std::thread([this]() {
        m_hook = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, nullptr, 0);
        if (!m_hook) {
            FileLog("[PTTListener] Failed to install hook\n");
            return;
        }
        FileLog("[PTTListener] Hook installed, pumping messages\n");

        MSG msg;
        while (m_running) {
            // MsgWaitForMultipleObjects lets us wake up periodically
            // to check m_running without burning CPU
            DWORD result = MsgWaitForMultipleObjects(
                0, nullptr, FALSE, 100, QS_ALLINPUT);

            if (result == WAIT_OBJECT_0) {
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }

        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
        FileLog("[PTTListener] Hook removed, thread exiting\n");
        });

    return true;
}

void PTTListener::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

LRESULT CALLBACK PTTListener::hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int k1 = s_instance->m_key1;
        int k2 = s_instance->m_key2;

        if ((int)kb->vkCode == k1 || (int)kb->vkCode == k2) {
            int mask = ((int)kb->vkCode == k1) ? 1 : 2;
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                bool wasActive = s_instance->m_activeKeys.load() > 0;
                s_instance->m_activeKeys |= mask;
                if (!wasActive && s_instance->m_callback) {
                    FileLog("[PTT]  ON");
                    s_instance->m_callback(true);
                }
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                s_instance->m_activeKeys &= ~mask;
                if (s_instance->m_activeKeys.load() == 0 && s_instance->m_callback) {
                    FileLog("[PTT] OFF");
                    s_instance->m_callback(false);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}