#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <fstream>

class WASAPILoopbackRecorder {
public:
    WASAPILoopbackRecorder();
    ~WASAPILoopbackRecorder();

    bool start(const std::string& outPath);  // starts loopback capture
    void stop();                              // flushes and closes file

private:
    void captureLoop();

    std::string m_outPath;
    std::thread m_thread;
    std::atomic<bool> m_running{ false };

    IMMDeviceEnumerator* m_enumerator = nullptr;
    IMMDevice*           m_device     = nullptr;
    IAudioClient*        m_client     = nullptr;
    IAudioCaptureClient* m_capture    = nullptr;

    // WAV file writing
    std::ofstream m_file;
    uint32_t m_dataBytes = 0;
    WAVEFORMATEX* m_wfx  = nullptr;

    void writeWavHeader();
    void patchWavHeader();
};