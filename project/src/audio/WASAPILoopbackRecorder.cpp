#include "WASAPILoopbackRecorder.h"
#include <iostream>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#pragma comment(lib, "ole32.lib")

using namespace std;

// WAV header layout
struct WavHeader {
    char     riff[4]      = {'R','I','F','F'};
    uint32_t fileSize     = 0;
    char     wave[4]      = {'W','A','V','E'};
    char     fmt[4]       = {'f','m','t',' '};
    uint32_t fmtSize      = 16;
    uint16_t audioFormat  = 1; // PCM
    uint16_t channels     = 0;
    uint32_t sampleRate   = 0;
    uint32_t byteRate     = 0;
    uint16_t blockAlign   = 0;
    uint16_t bitsPerSample= 0;
    char     data[4]      = {'d','a','t','a'};
    uint32_t dataSize     = 0;
};

WASAPILoopbackRecorder::WASAPILoopbackRecorder() {}

WASAPILoopbackRecorder::~WASAPILoopbackRecorder() { stop(); }

bool WASAPILoopbackRecorder::start(const string& outPath) {
    m_outPath = outPath;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Get default render (playback) device - works with ANY device including USB headset
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator), (void**)&m_enumerator);
    m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);

    // Activate audio client in LOOPBACK mode
    m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_client);

    m_client->GetMixFormat(&m_wfx);

    // AUDCLNT_STREAMFLAGS_LOOPBACK is the key flag - captures what's playing
    HRESULT hr = m_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000, 0, m_wfx, nullptr);

    if (FAILED(hr)) {
        cout << "[WASAPILoopback] Initialize failed: " << hex << hr << "\n";
        return false;
    }

    m_client->GetService(__uuidof(IAudioCaptureClient), (void**)&m_capture);

    // Open WAV output file
    m_file.open(outPath, ios::binary);
    writeWavHeader(); // write placeholder header, patched on stop()

    m_running = true;
    m_client->Start();
    m_thread = thread(&WASAPILoopbackRecorder::captureLoop, this);

    cout << "[WASAPILoopback] Recording started => " << outPath << "\n";
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

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // Write silence explicitly so timing stays correct
                vector<BYTE> silence(bytes, 0);
                m_file.write((char*)silence.data(), bytes);
            } else {
                m_file.write((char*)data, bytes);
            }
            m_dataBytes += bytes;

            m_capture->ReleaseBuffer(frames);
            m_capture->GetNextPacketSize(&packetSize);
        }
    }
}

void WASAPILoopbackRecorder::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_thread.joinable()) m_thread.join();
    if (m_client) m_client->Stop();

    patchWavHeader(); // fix file size fields now that we know total bytes
    m_file.close();

    if (m_capture)    { m_capture->Release();    m_capture    = nullptr; }
    if (m_client)     { m_client->Release();      m_client     = nullptr; }
    if (m_device)     { m_device->Release();      m_device     = nullptr; }
    if (m_enumerator) { m_enumerator->Release();  m_enumerator = nullptr; }
    if (m_wfx)        { CoTaskMemFree(m_wfx);     m_wfx        = nullptr; }

    cout << "[WASAPILoopback] Saved => " << m_outPath 
         << " (" << m_dataBytes << " bytes)\n";
}

void WASAPILoopbackRecorder::writeWavHeader() {
    WavHeader h;
    h.audioFormat   = 3;                      // 3 = IEEE_FLOAT, not 1 (PCM integer)
    h.channels      = m_wfx->nChannels;
    h.sampleRate    = m_wfx->nSamplesPerSec;
    h.bitsPerSample = m_wfx->wBitsPerSample;  // will be 32
    h.blockAlign    = m_wfx->nBlockAlign;
    h.byteRate      = m_wfx->nAvgBytesPerSec;
    m_file.write((char*)&h, sizeof(h));
}

void WASAPILoopbackRecorder::patchWavHeader() {
    uint32_t fileSize = 36 + m_dataBytes;
    m_file.seekp(4);  m_file.write((char*)&fileSize,    4);
    m_file.seekp(40); m_file.write((char*)&m_dataBytes, 4);
}