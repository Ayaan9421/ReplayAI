#include "MicRollingBuffer.h"
#include <iostream>
#include <fstream>
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "ole32.lib")

using namespace std;

#pragma pack(push, 1)
struct WavHeaderExtensible {
    // RIFF chunk
    char     riff[4]        = {'R','I','F','F'};
    uint32_t fileSize       = 0;
    char     wave[4]        = {'W','A','V','E'};
    // fmt chunk — 40 bytes for extensible
    char     fmt[4]         = {'f','m','t',' '};
    uint32_t fmtSize        = 40;
    uint16_t audioFormat    = 0xFFFE;  // WAVE_FORMAT_EXTENSIBLE
    uint16_t channels       = 0;
    uint32_t sampleRate     = 0;
    uint32_t byteRate       = 0;
    uint16_t blockAlign     = 0;
    uint16_t bitsPerSample  = 0;
    // Extension fields
    uint16_t cbSize         = 22;
    uint16_t validBitsPerSample = 0;
    uint32_t channelMask    = 3;  // front left + front right
    // SubFormat GUID for IEEE_FLOAT:
    // {00000003-0000-0010-8000-00AA00389B71}
    uint8_t  subFormat[16]  = {
        0x03,0x00,0x00,0x00,
        0x00,0x00,
        0x10,0x00,
        0x80,0x00,
        0x00,0xAA,0x00,0x38,0x9B,0x71
    };
    // data chunk
    char     data[4]        = {'d','a','t','a'};
    uint32_t dataSize       = 0;
};
#pragma pack(pop)


MicRollingBuffer::MicRollingBuffer() {}
MicRollingBuffer::~MicRollingBuffer() { stop(); }

bool MicRollingBuffer::start(double maxDurationSec) {
    m_maxDurationSec = maxDurationSec;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator), (void**)&m_enumerator);

    // Enumerate capture devices and find USB mic by name
    IMMDeviceCollection* collection = nullptr;
    m_enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
        IMMDevice* dev = nullptr;
        collection->Item(i, &dev);

        IPropertyStore* props = nullptr;
        dev->OpenPropertyStore(STGM_READ, &props);

        PROPVARIANT name;
        PropVariantInit(&name);
        props->GetValue(PKEY_Device_FriendlyName, &name);

        // Hardcoded for now — GUI will expose this as a setting later
        bool isUSBMic = (wcsstr(name.pwszVal, L"USB Audio Device") != nullptr);

        PropVariantClear(&name);
        props->Release();

        if (isUSBMic) {
            m_device = dev;
            cout << "[MicBuffer] Found USB mic\n";
            break;
        }
        dev->Release();
    }
    collection->Release();

    if (!m_device) {
        cout << "[MicBuffer] ERROR: USB mic not found\n";
        return false;
    }

    m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_client);
    m_client->GetMixFormat(&m_wfx);

    cout << "[MicBuffer] Format: " << m_wfx->nChannels << "ch "
         << m_wfx->nSamplesPerSec << "Hz "
         << m_wfx->wBitsPerSample << "bit tag=" << m_wfx->wFormatTag << "\n";

    HRESULT hr = m_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED, 0,
        10000000, 0, m_wfx, nullptr);

    if (FAILED(hr)) {
        cout << "[MicBuffer] Initialize failed: " << hex << hr << "\n";
        return false;
    }

    m_client->GetService(__uuidof(IAudioCaptureClient), (void**)&m_capture);

    m_running = true;
    m_client->Start();
    m_thread = thread(&MicRollingBuffer::captureLoop, this);

    cout << "[MicBuffer] Rolling mic buffer started (" << maxDurationSec << "s)\n";
    return true;
}

void MicRollingBuffer::setPTT(bool active) {
    m_pttActive = active;
}

void MicRollingBuffer::captureLoop() {
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

            MicChunk chunk;
            chunk.durationSec = chunkDuration;
            chunk.capturedAt  = std::chrono::steady_clock::now();
            chunk.pttActive   = m_pttActive.load();
            chunk.data.resize(bytes);

            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !chunk.pttActive) {
                // silence if PTT off OR hardware silent
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

void MicRollingBuffer::trimIfNeeded() {
    while (m_currentDurationSec > m_maxDurationSec && !m_buffer.empty()) {
        m_currentDurationSec -= m_buffer.front().durationSec;
        m_buffer.pop_front();
    }
}

bool MicRollingBuffer::saveSnapshot(const string& outPath,
    std::chrono::steady_clock::time_point windowStartWallClock) {

    deque<MicChunk> snapshot;
    {
        lock_guard<mutex> lock(m_mutex);
        snapshot = m_buffer;
    }

    if (snapshot.empty()) {
        cout << "[MicBuffer] Buffer empty\n";
        return false;
    }

    // Trim chunks before video window start — same as WASAPILoopbackRecorder
    while (!snapshot.empty() &&
           snapshot.front().capturedAt < windowStartWallClock) {
        snapshot.pop_front();
    }

    // Chunks already have silence where PTT was off — just write them all
    writeWav(outPath, snapshot);

    double duration = 0;
    int pttChunks = 0;
    for (auto& c : snapshot) {
        duration += c.durationSec;
        if (c.pttActive) pttChunks++;
    }

    cout << "[MicBuffer] Snapshot saved => " << outPath
         << " (" << duration << "s, PTT active in "
         << pttChunks << "/" << snapshot.size() << " chunks)\n";
    return true;
}

void MicRollingBuffer::writeWav(const string& path, const deque<MicChunk>& chunks) {
    // Convert float32 samples to int16 PCM for reliable WAV output
    // Both game audio and mic are 48kHz stereo — int16 is universally readable
    
    struct WavHeaderPCM {
        char     riff[4]       = {'R','I','F','F'};
        uint32_t fileSize      = 0;
        char     wave[4]       = {'W','A','V','E'};
        char     fmt[4]        = {'f','m','t',' '};
        uint32_t fmtSize       = 16;
        uint16_t audioFormat   = 1;   // PCM integer
        uint16_t channels      = 0;
        uint32_t sampleRate    = 0;
        uint32_t byteRate      = 0;
        uint16_t blockAlign    = 0;
        uint16_t bitsPerSample = 16;
        char     data[4]       = {'d','a','t','a'};
        uint32_t dataSize      = 0;
    };

    // Count total float samples
    uint32_t totalFloatBytes = 0;
    for (auto& c : chunks) totalFloatBytes += (uint32_t)c.data.size();
    
    // Each float (4 bytes) -> int16 (2 bytes)
    uint32_t totalPCMBytes = totalFloatBytes / 2;

    WavHeaderPCM h;
    h.channels      = m_wfx->nChannels;
    h.sampleRate    = m_wfx->nSamplesPerSec;
    h.bitsPerSample = 16;
    h.blockAlign    = h.channels * 2;
    h.byteRate      = h.sampleRate * h.blockAlign;
    h.dataSize      = totalPCMBytes;
    h.fileSize      = sizeof(h) - 8 + totalPCMBytes;

    ofstream f(path, ios::binary);
    f.write((char*)&h, sizeof(h));

    // Convert float32 -> int16 chunk by chunk
    for (auto& c : chunks) {
        const float* floatData = reinterpret_cast<const float*>(c.data.data());
        uint32_t numSamples = (uint32_t)c.data.size() / sizeof(float);
        
        vector<int16_t> pcm(numSamples);
        for (uint32_t i = 0; i < numSamples; i++) {
            float s = floatData[i];
            // Clamp to [-1, 1] then scale to int16 range
            if (s >  1.0f) s =  1.0f;
            if (s < -1.0f) s = -1.0f;
            pcm[i] = static_cast<int16_t>(s * 32767.0f);
        }
        f.write((char*)pcm.data(), pcm.size() * sizeof(int16_t));
    }

    cout << "[MicBuffer] WAV written: " << path
         << " PCM16 " << h.sampleRate << "Hz "
         << h.channels << "ch " << totalPCMBytes << " bytes\n";
}

void MicRollingBuffer::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    if (m_client)     { m_client->Stop(); m_client->Release();      m_client     = nullptr; }
    if (m_capture)    { m_capture->Release();                        m_capture    = nullptr; }
    if (m_device)     { m_device->Release();                         m_device     = nullptr; }
    if (m_enumerator) { m_enumerator->Release();                     m_enumerator = nullptr; }
    if (m_wfx)        { CoTaskMemFree(m_wfx);                        m_wfx        = nullptr; }
    cout << "[MicBuffer] Stopped\n";
}