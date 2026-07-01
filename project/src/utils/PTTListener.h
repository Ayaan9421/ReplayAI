#pragma once
#include <windows.h>
#include <functional>
#include <atomic>

// Passive low-level keyboard hook — monitors T and U
// without consuming them (game still receives the keys)
class PTTListener {
public:
    // callbacks fired on key down/up
    using Callback = std::function<void(bool pttActive)>;

    bool start(Callback onPTTChange);
    void stop();

    // Called internally by the hook proc — don't call directly
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    static PTTListener* s_instance;
    HHOOK m_hook = nullptr;
    Callback m_callback;
    std::atomic<uint32_t> m_activeKeys{ 0 };  // count of T/U currently held
};