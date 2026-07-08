#pragma once
#include <windows.h>
#include <functional>
#include <atomic>
#include <thread>

class PTTListener {
public:
    using Callback = std::function<void(bool)>;

    bool start(Callback onPTTChange);
    void stop();
    void setKeys(int key1, int key2) { m_key1 = key1; m_key2 = key2; }

private:
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static PTTListener* s_instance;
    int m_key1 = 0x54;
    int m_key2 = 0x55;
    HHOOK              m_hook = nullptr;
    Callback           m_callback;
    std::atomic<int>   m_activeKeys{ 0 };
    std::atomic<bool>  m_running{ false };
    std::thread        m_thread;
};