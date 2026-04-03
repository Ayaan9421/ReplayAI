#include "WASAPILoopbackRecorder.h"
#include <iostream>
#include <fstream>
#include <initguid.h>

#pragma comment(lib, "ole32.lib")

using namespace std;

struct WavHeader {
    char     riff[4]       = {'R','I','F','F'};
    uint32_t fileSize      = 0;
    char     wave[4]       = {'W','A','V','E'};
    char     fmt[4]        = {'f','m','t',' '};
    uint32_t fmtSize       = 16;
    uint16_t audioFormat   = 3;   // IEEE_FLOAT
    uint16_t channels      = 0;
    uint32_t sampleRate    = 0;
    uint32_t byteRate      = 0;
    uint16_t blockAlign    = 0;
    uint16_t bitsPerSample = 0;
    char     data[4]       = {'d','a','t','a'};
    uint32_t dataSize      = 0;
};

WASAPILoopbackRecorder::WASAPILoopbackRecorder() {}
WASAPILoopbackRecorder::~WASAPILoopbackRecorder() { stop(); }

bool WASAPILoopbackRecorder::start(double maxDurationSec) {
    m_maxDurationSec = maxDurationSec;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator), (void**)&m_enumerator);
    m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_client);
    m_client->GetMixFormat(&m_wfx);

    HRESULT hr = m_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000, 0, m_wfx, nullptr);

    if (FAILED(hr)) {
        cout << "[WASAPILoopback] Initialize failed: " << hex << hr << "\n";
        return false;
    }

    m_client->GetService(__uuidof(IAudioCaptureClient), (void**)&m_capture);

    m_running = true;
    m_client->Start();
    m_thread = thread(&WASAPILoopbackRecorder::captureLoop, this);

    cout << "[WASAPILoopback] Rolling buffer started (" << maxDurationSec << "s)\n";
    return true;
}

void WASAPILoopbackRecorder::captureLoop() {
    while (m_running) {
        Sleep(10);

        UINT32 packetSize = 0;
        m_capture->GetNextPacketSize(&packetSize);

        while (packetSize > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;

            m_capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);

            UINT32 bytes = frames * m_wfx->nBlockAlign;
            double chunkDuration = (double)frames / m_wfx->nSamplesPerSec;

            AudioChunk chunk;
            chunk.durationSec = chunkDuration;
            chunk.data.resize(bytes);

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // silence — zero fill so timing stays correct
                fill(chunk.data.begin(), chunk.data.end(), 0);
            } else {
                memcpy(chunk.data.data(), data, bytes);
            }

            m_capture->ReleaseBuffer(frames);

            {
                lock_guard<mutex> lock(m_mutex);
                m_buffer.push_back(move(chunk));
                m_currentDurationSec += chunkDuration;
                trimIfNeeded();
            }

            m_capture->GetNextPacketSize(&packetSize);
        }
    }
}

void WASAPILoopbackRecorder::trimIfNeeded() {
    // mirrors RollingBufferManager::trimIfNeeded exactly
    while (m_currentDurationSec > m_maxDurationSec && !m_buffer.empty()) {
        m_currentDurationSec -= m_buffer.front().durationSec;
        m_buffer.pop_front();
    }
}

bool WASAPILoopbackRecorder::saveSnapshot(const string& outPath) {
    deque<AudioChunk> snapshot;
    {
        lock_guard<mutex> lock(m_mutex);
        snapshot = m_buffer;  // copy current rolling window
    }

    if (snapshot.empty()) {
        cout << "[WASAPILoopback] Buffer empty, nothing to save\n";
        return false;
    }

    writeWav(outPath, snapshot);

    double duration = 0;
    for (auto& c : snapshot) duration += c.durationSec;
    cout << "[WASAPILoopback] Snapshot saved => " << outPath
         << " (" << duration << "s)\n";
    return true;
}

void WASAPILoopbackRecorder::writeWav(const string& path, const deque<AudioChunk>& chunks) {
    uint32_t totalBytes = 0;
    for (auto& c : chunks) totalBytes += (uint32_t)c.data.size();

    WavHeader h;
    h.channels      = m_wfx->nChannels;
    h.sampleRate    = m_wfx->nSamplesPerSec;
    h.bitsPerSample = m_wfx->wBitsPerSample;
    h.blockAlign    = m_wfx->nBlockAlign;
    h.byteRate      = m_wfx->nAvgBytesPerSec;
    h.dataSize      = totalBytes;
    h.fileSize      = 36 + totalBytes;

    ofstream f(path, ios::binary);
    f.write((char*)&h, sizeof(h));
    for (auto& c : chunks) f.write((char*)c.data.data(), c.data.size());
}

void WASAPILoopbackRecorder::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    if (m_client)     { m_client->Stop(); m_client->Release();      m_client     = nullptr; }
    if (m_capture)    { m_capture->Release();                        m_capture    = nullptr; }
    if (m_device)     { m_device->Release();                         m_device     = nullptr; }
    if (m_enumerator) { m_enumerator->Release();                     m_enumerator = nullptr; }
    if (m_wfx)        { CoTaskMemFree(m_wfx);                        m_wfx        = nullptr; }
    cout << "[WASAPILoopback] Stopped\n";
}