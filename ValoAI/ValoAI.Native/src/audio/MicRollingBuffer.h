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
#include <chrono>

struct MicChunk {
    std::vector<uint8_t> data;
    double durationSec;
    std::chrono::steady_clock::time_point capturedAt;
    bool pttActive;  // was PTT held when this chunk was captured
};

class MicRollingBuffer {
public:
    MicRollingBuffer();
    ~MicRollingBuffer();

    bool start(double maxDurationSec = 60.0, const std::string& deviceName = "Default");
    void stop();

    void setPTT(bool active);  // called from keyboard hook

    // Save last N seconds of mic — silence where PTT was off
    bool saveSnapshot(const std::string& outPath,
                      std::chrono::steady_clock::time_point windowStartWallClock);

private:
    void captureLoop();
    void trimIfNeeded();
    void writeWav(const std::string& path, const std::deque<MicChunk>& chunks);

    double m_maxDurationSec = 60.0;
    double m_currentDurationSec = 0.0;

    std::atomic<bool> m_pttActive{ false };

    std::deque<MicChunk> m_buffer;
    std::mutex m_mutex;

    std::thread m_thread;
    std::atomic<bool> m_running{ false };

    IMMDeviceEnumerator* m_enumerator = nullptr;
    IMMDevice*           m_device     = nullptr;
    IAudioClient*        m_client     = nullptr;
    IAudioCaptureClient* m_capture    = nullptr;
    WAVEFORMATEX*        m_wfx        = nullptr;
};

