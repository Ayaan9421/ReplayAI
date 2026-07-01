#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>


#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

using namespace std;

// ---- WAV Header (extensible float) ----
#pragma pack(push, 1)
struct WavHeaderExtensible {
    char     riff[4]            = {'R','I','F','F'};
    uint32_t fileSize           = 0;
    char     wave[4]            = {'W','A','V','E'};
    char     fmt[4]             = {'f','m','t',' '};
    uint32_t fmtSize            = 40;
    uint16_t audioFormat        = 0xFFFE;
    uint16_t channels           = 0;
    uint32_t sampleRate         = 0;
    uint32_t byteRate           = 0;
    uint16_t blockAlign         = 0;
    uint16_t bitsPerSample      = 0;
    uint16_t cbSize             = 22;
    uint16_t validBitsPerSample = 0;
    uint32_t channelMask        = 3;
    uint8_t  subFormat[16]      = {
        0x03,0x00,0x00,0x00,
        0x00,0x00,
        0x10,0x00,
        0x80,0x00,
        0x00,0xAA,0x00,0x38,0x9B,0x71
    };
    char     data[4]            = {'d','a','t','a'};
    uint32_t dataSize           = 0;
};
#pragma pack(pop)

// ---- Globals ----
atomic<bool>     g_pttActive{ false };
atomic<bool>     g_running{ true };
HHOOK            g_hook = nullptr;

// ---- PTT Hook ----
LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        // T = 0x54
        if (kb->vkCode == 0x54) {
            if (wParam == WM_KEYDOWN) {
                if (!g_pttActive) {
                    g_pttActive = true;
                    cout << "[PTT] ON\n";
                }
            } else if (wParam == WM_KEYUP) {
                g_pttActive = false;
                cout << "[PTT] OFF\n";
            }
        }
        // ESC to stop
        if (kb->vkCode == VK_ESCAPE && wParam == WM_KEYDOWN) {
            g_running = false;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // ---- Setup mic ----
    IMMDeviceEnumerator* enumerator = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator), (void**)&enumerator);

    IMMDeviceCollection* collection = nullptr;
    enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);

    UINT count = 0;
    collection->GetCount(&count);

    cout << "Available capture devices:\n";
    for (UINT i = 0; i < count; i++) {
        IMMDevice* dev = nullptr;
        collection->Item(i, &dev);

        IPropertyStore* props = nullptr;
        dev->OpenPropertyStore(STGM_READ, &props);

        PROPVARIANT name;
        PropVariantInit(&name);
        props->GetValue(PKEY_Device_FriendlyName, &name);

        cout << "  [" << i << "] " << name.pwszVal << "\n";

        PropVariantClear(&name);
        props->Release();
        dev->Release();
    }

    cout << "Select device index: ";
    int idx = 0;
    cin >> idx;

    IMMDevice* device = nullptr;
    collection->Item(idx, &device);
    collection->Release();

    // Print selected device name
    IPropertyStore* props = nullptr;
    device->OpenPropertyStore(STGM_READ, &props);
    PROPVARIANT name;
    PropVariantInit(&name);
    props->GetValue(PKEY_Device_FriendlyName, &name);
    wcout << L"Using: " << name.pwszVal << L"\n";
    PropVariantClear(&name);
    props->Release();

    IAudioClient* client = nullptr;
    device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);

    WAVEFORMATEX* wfx = nullptr;
    client->GetMixFormat(&wfx);

    cout << "Mic format: " << wfx->nChannels << "ch "
         << wfx->nSamplesPerSec << "Hz "
         << wfx->wBitsPerSample << "bit "
         << "tag=" << wfx->wFormatTag << "\n";

    HRESULT hr = client->Initialize(
        AUDCLNT_SHAREMODE_SHARED, 0,
        10000000, 0, wfx, nullptr);

    if (FAILED(hr)) {
        cout << "IAudioClient::Initialize failed: " << hex << hr << "\n";
        return -1;
    }

    IAudioCaptureClient* capture = nullptr;
    client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
    client->Start();

    // ---- Install PTT hook ----
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, nullptr, 0);
    if (!g_hook) {
        cout << "Hook failed!\n";
        return -1;
    }

    cout << "\n=== Mic test running ===\n";
    cout << "Hold T to talk (PTT)\n";
    cout << "Press ESC to stop and save mic.wav\n\n";

    // ---- Capture loop ----
    vector<uint8_t> allAudio;
    allAudio.reserve(48000 * 8 * 60); // pre-alloc 60s

    auto startTime = chrono::steady_clock::now();

    while (g_running) {
        // Pump messages so hook fires
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        UINT32 packetSize = 0;
        capture->GetNextPacketSize(&packetSize);

        while (packetSize > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;

            capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);

            UINT32 bytes = frames * wfx->nBlockAlign;

            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !g_pttActive) {
                // silence when PTT off
                vector<uint8_t> silence(bytes, 0);
                allAudio.insert(allAudio.end(), silence.begin(), silence.end());
            } else {
                allAudio.insert(allAudio.end(), data, data + bytes);
            }

            capture->ReleaseBuffer(frames);
            capture->GetNextPacketSize(&packetSize);
        }

        Sleep(5);
    }

    double duration = chrono::duration<double>(
        chrono::steady_clock::now() - startTime).count();
    cout << "\nStopping... captured " << duration << "s\n";

    // ---- Write WAV ----
    WavHeaderExtensible h;
    h.channels           = wfx->nChannels;
    h.sampleRate         = wfx->nSamplesPerSec;
    h.bitsPerSample      = wfx->wBitsPerSample;
    h.blockAlign         = wfx->nBlockAlign;
    h.byteRate           = wfx->nAvgBytesPerSec;
    h.validBitsPerSample = wfx->wBitsPerSample;
    h.dataSize           = (uint32_t)allAudio.size();
    h.fileSize           = sizeof(WavHeaderExtensible) - 8 + h.dataSize;

    ofstream f("mic_test.wav", ios::binary);
    f.write((char*)&h, sizeof(h));
    f.write((char*)allAudio.data(), allAudio.size());
    f.close();

    cout << "Saved mic_test.wav (" << allAudio.size() << " bytes)\n";
    cout << "Open mic_test.wav in VLC and check if you hear your voice during PTT\n";

    // Cleanup
    UnhookWindowsHookEx(g_hook);
    client->Stop();
    capture->Release();
    client->Release();
    CoTaskMemFree(wfx);
    device->Release();
    enumerator->Release();
    CoUninitialize();
    return 0;
}