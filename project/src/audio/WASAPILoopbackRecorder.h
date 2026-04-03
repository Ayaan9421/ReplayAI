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

struct AudioChunk {
    std::vector<uint8_t> data;  // raw PCM bytes
    double durationSec;         // duration of this chunk
};

class WASAPILoopbackRecorder {
public:
    WASAPILoopbackRecorder();
    ~WASAPILoopbackRecorder();

    bool start(double maxDurationSec = 60.0);
    void stop();

    // Call on hotkey — writes last N seconds to WAV file
    bool saveSnapshot(const std::string& outPath);

private:
    void captureLoop();
    void trimIfNeeded();
    void writeWav(const std::string& path, const std::deque<AudioChunk>& chunks);

    double m_maxDurationSec = 60.0;
    double m_currentDurationSec = 0.0;

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