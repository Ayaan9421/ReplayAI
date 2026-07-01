#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <thread>
#include <atomic>
#include <deque>
#include <vector>
#include <mutex>
#include <fstream>
#include <chrono>

struct AudioChunk {
    std::vector<uint8_t> data;
    double durationSec;
    std::chrono::steady_clock::time_point capturedAt;
};

class WASAPILoopbackRecorder {
public:
    WASAPILoopbackRecorder();
    ~WASAPILoopbackRecorder();

    bool start(double maxDurationSec = 60.0);
    void stop();

    bool saveSnapshot(const std::string& outPath,
                      std::chrono::steady_clock::time_point windowStartWallClock);

private:
    void captureLoop();
    void trimIfNeeded();
    void writeWav(const std::string& path, const std::deque<AudioChunk>& chunks);

    double m_maxDurationSec = 60.0;
    double m_currentDurationSec = 0.0;

    // Audio capture start wall clock — set once when start() is called
    std::chrono::steady_clock::time_point m_captureStartTime;

    std::deque<AudioChunk> m_buffer;
    std::mutex m_mutex;

    std::thread m_thread;
    std::atomic<bool> m_running{ false };

    IMMDeviceEnumerator* m_enumerator = nullptr;
    IMMDevice*           m_device     = nullptr;
    IAudioClient*        m_client     = nullptr;
    IAudioCaptureClient* m_capture    = nullptr;
    WAVEFORMATEX*        m_wfx        = nullptr;
};
