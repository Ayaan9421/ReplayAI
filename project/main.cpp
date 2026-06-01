#include <winrt/windows.foundation.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include "audio/WASAPILoopbackRecorder.h"
#include "buffer/RollingBufferManager.h"
#include "capture/ScreenCaptureWGC.h"
#include "core/D3DContext.h"
#include "encoder/IVideoEncoder.h"
#include "encoder/NVEncoder.h"
#include "utils/FFmpegAudioRecorder.h"
#include "utils/FFmpegMuxer.h"
#include "utils/HotkeyListener.h"

using namespace std;

// time to build UI with WPF 

#include <iomanip>
#include <sstream>

std::string getTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
    localtime_s(&tm, &t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

int main() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    const double FPS = 60.0;
    D3DContext d3d;
    if (!d3d.initialize()) {
        cout << "Failed" << endl;
        return -1;
    }

    ScreenCaptureWGC cap;
    if (!cap.initialize(&d3d)) {
        cout << "WGC init failed" << endl;
        return -1;
    }

    unique_ptr<IVideoEncoder> encoder = make_unique<NVEncoder>();
    if (!encoder->initialize(d3d.device(), 1920, 1080)) {
        cout << "[Encoder] init failed" << endl;
        return -1;
    }

    WASAPILoopbackRecorder audio;
    audio.start(60.0);

    RollingBufferManager videoBuffer(FPS);
    HotkeyListener hotkey;
    if (!hotkey.initialize()) { return -1; }

    cout << "[Main] Capture started, waiting for frames...\n";

    auto startTime = std::chrono::steady_clock::now();
    MSG msg{};
    constexpr double FRAME_DURATION = 1.0 / 60.0;

    while (true) {
        if (hotkey.poll()) {
            cout << "[Hotkey] pressed! Saving Last 60 seconds" << endl;

            std::string ts = getTimestamp();

            std::string tempVideo = "temp_" + ts + ".h264";
            std::string tempAudio = "audio_" + ts + ".wav";
            std::string finalClip = "clip_" + ts + ".mp4";

            auto videoshot = videoBuffer.snapshot();

            videoBuffer.writeToFile(videoshot, tempVideo);
            audio.saveSnapshot(tempAudio);

            if (FFmpegMuxer::mux(tempVideo, tempAudio, finalClip)) {
                cout << "[Hotkey] Clip Saved: " << finalClip << endl;

                std::filesystem::remove(tempVideo);
                std::filesystem::remove(tempAudio);
            }
            else {
                cout << "[Muxer] Failed!" << endl;
            }
        }
        ID3D11Texture2D* frame = cap.getFrame();
        if (frame) {
            vector<uint8_t> h264;
            bool isKeyframe = false;
            if (encoder->encodeFrame(frame, h264, isKeyframe) && !h264.empty()) {
                EncodedFragment frag;
                frag.data        = h264;
                frag.durationSec = FRAME_DURATION;
                frag.isIDR       = isKeyframe;
                videoBuffer.pushFragment(frag);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    encoder->shutdown();
    cout << "Done\n";

    return 0;
}