#pragma once
#include <windows.h>

class HotkeyListener {
   public:
    HotkeyListener();
    ~HotkeyListener();

    // Registers Ctrl + Shift + F9 (customizable later)
    bool initialize();

    // Call every frame / loop iteration
    bool poll();  // returns true if hotkey was pressed

   private:
    static constexpr int HOTKEY_ID = 1;
};
