#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

vector<string> listDShowAudioDevices() {
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

int main() {
    vector<string> dev = listDShowAudioDevices();
    for (auto it : dev) { cout << it << endl; }
    return 0;
}