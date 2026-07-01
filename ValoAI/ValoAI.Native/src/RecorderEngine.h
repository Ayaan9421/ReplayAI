#pragma once
#include <string>
#include <functional>
#include <deque>

struct ClipResult {
    std::string filePath;
    std::string thumbnailPath;
    double      durationSec;
};

struct EngineSettings {
    std::string clipsFolder;
    int         bufferDurationSec = 60;
    int         targetFps = 60;
    int         pttKey1 = 0x54;
    int         pttKey2 = 0x55;
    bool        micEnabled = true;
};

class RecorderEngine {
public:
    RecorderEngine();
    ~RecorderEngine();

    bool start(const EngineSettings& settings);
    void stop();
    bool saveClip(ClipResult& outResult);

    // Called every ~0.5ms from the capture thread
    void tick();

    bool isRunning() const { return m_running; }

private:
    struct Impl;
    Impl* m_impl = nullptr;
    bool  m_running = false;
};