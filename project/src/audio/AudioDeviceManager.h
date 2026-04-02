#pragma once
#include <string>
#include <vector>

class AudioDeviceManager {
   public:
    static std::vector<std::string> listDShowAudioDevices();
};
