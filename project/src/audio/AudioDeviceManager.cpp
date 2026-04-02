#include "AudioDeviceManager.h"

#include <cstdio>
#include <iostream>
#include <regex>
#include <string>

using namespace std;

vector<string> AudioDeviceManager::listDShowAudioDevices() {
    vector<string> devices;

    FILE* pipe = _popen("ffmpeg -f dshow -list_devices true -i dummy 2>&1", "r");
    if (!pipe) return devices;

    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), pipe)) {
        string line(buffer);

        if (line.find("(audio)") != string::npos) {
            auto first = line.find('"');
            auto second = line.find('"', first + 1);
            if (first != string::npos && second != string::npos) {
                devices.push_back(line.substr(first + 1, second - first - 1));
            }
        }
    }

    _pclose(pipe);
    return devices;
}