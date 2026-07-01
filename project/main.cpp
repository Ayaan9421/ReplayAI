// #include <winrt/windows.foundation.h>
// #include <filesystem>
// #include <fstream>
// #include <iostream>
// #include <thread>
// #include <iomanip>
// #include <sstream>

// #include "audio/WASAPILoopbackRecorder.h"
// #include "buffer/RollingBufferManager.h"
// #include "capture/ScreenCaptureWGC.h"
// #include "core/D3DContext.h"
// #include "encoder/IVideoEncoder.h"
// #include "encoder/NVEncoder.h"
// #include "utils/FFmpegMuxer.h"
// #include "utils/HotkeyListener.h"

// using namespace std;

// std::string getTimestamp() {
//     auto now = std::chrono::system_clock::now();
//     auto t = std::chrono::system_clock::to_time_t(now);
//     std::tm tm{};
//     localtime_s(&tm, &t);
//     std::ostringstream oss;
//     oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
//     return oss.str();
// }

// // TODO: LETSSSS FRIGGINGGGG GOOOO AUDIO VIDEO SYNCED
// // NEXT STEP, ADD MIC CAPTURE. GUI. ML
// int main() {
//     winrt::init_apartment(winrt::apartment_type::multi_threaded);

//     D3DContext d3d;
//     if (!d3d.initialize()) { cout << "Failed\n"; return -1; }

//     ScreenCaptureWGC cap;
//     if (!cap.initialize(&d3d)) { cout << "WGC init failed\n"; return -1; }

//     unique_ptr<IVideoEncoder> encoder = make_unique<NVEncoder>();
//     if (!encoder->initialize(d3d.device(), 1920, 1080)) {
//         cout << "[Encoder] init failed\n"; return -1;
//     }

//     WASAPILoopbackRecorder audio;
//     audio.start(60.0);

//     RollingBufferManager videoBuffer(60.0);
//     HotkeyListener hotkey;
//     if (!hotkey.initialize()) { return -1; }

//     cout << "[Main] Capture started, waiting for frames...\n";

//     auto lastFrameTime = std::chrono::steady_clock::now();
//     std::chrono::steady_clock::time_point windowStartWallClock =
//         std::chrono::steady_clock::now();

//     while (true) {
//         if (hotkey.poll()) {
//             cout << "[Hotkey] pressed! Saving Last 60 seconds\n";

//             std::string ts = getTimestamp();
//             std::string tempVideo = "temp_"  + ts + ".h264";
//             std::string tempAudio = "audio_" + ts + ".wav";
//             std::string finalClip = "clip_"  + ts + ".mp4";

//             auto videoshot = videoBuffer.snapshot();
//             videoBuffer.writeToFile(videoshot, tempVideo);

//             auto now2 = std::chrono::steady_clock::now();
//             double windowAge = std::chrono::duration<double>(now2 - windowStartWallClock).count();
//             cout << "[Main] windowStartWallClock age = " << windowAge << "s\n";

//             audio.saveSnapshot(tempAudio, windowStartWallClock);

//             if (FFmpegMuxer::mux(tempVideo, tempAudio, finalClip)) {
//                 cout << "[Hotkey] Clip Saved: " << finalClip << "\n";
//                 std::filesystem::remove(tempVideo);
//                 std::filesystem::remove(tempAudio);
//             } else {
//                 cout << "[Muxer] Failed!\n";
//             }
//         }

//         if (cap.hasNewFrame()) {
//             ID3D11Texture2D* frame = cap.getFrame();
//             if (frame) {
//                 auto now = std::chrono::steady_clock::now();
//                 double realElapsed = std::chrono::duration<double>(
//                     now - lastFrameTime).count();
//                 lastFrameTime = now;

//                 if (realElapsed > 0.5) realElapsed = 1.0 / 60.0;

//                 vector<uint8_t> h264;
//                 bool isKeyframe = false;
//                 if (encoder->encodeFrame(frame, h264, isKeyframe) && !h264.empty()) {
//                     EncodedFragment frag;
//                     frag.data        = h264;
//                     frag.durationSec = realElapsed;
//                     frag.isIDR       = isKeyframe;
//                     videoBuffer.pushFragment(frag);

//                     // Update window start: if buffer is full, window start
//                     // is now - bufferDuration. If not full, it's recording start.
//                     double bufDuration = videoBuffer.currentDuration();
//                     auto bufDur = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
//                         std::chrono::duration<double>(bufDuration));
//                     windowStartWallClock = now - bufDur;
//                 }
//             }
//         }

//         std::this_thread::sleep_for(std::chrono::microseconds(500));
//     }

//     encoder->shutdown();
//     cout << "Done\n";
//     return 0;
// }


#include <winrt/windows.foundation.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <iomanip>
#include <sstream>

#include "audio/WASAPILoopbackRecorder.h"
#include "audio/MicRollingBuffer.h"
#include "buffer/RollingBufferManager.h"
#include "capture/ScreenCaptureWGC.h"
#include "core/D3DContext.h"
#include "encoder/IVideoEncoder.h"
#include "encoder/NVEncoder.h"
#include "utils/FFmpegMuxer.h"
#include "utils/HotkeyListener.h"
#include "utils/PTTListener.h"

using namespace std;

std::string getTimestamp() {
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

    D3DContext d3d;
    if (!d3d.initialize()) { cout << "Failed\n"; return -1; }

    ScreenCaptureWGC cap;
    if (!cap.initialize(&d3d)) { cout << "WGC init failed\n"; return -1; }

    unique_ptr<IVideoEncoder> encoder = make_unique<NVEncoder>();
    if (!encoder->initialize(d3d.device(), 1920, 1080)) {
        cout << "[Encoder] init failed\n"; return -1;
    }

    WASAPILoopbackRecorder audio;
    audio.start(60.0);

    MicRollingBuffer mic;
    mic.start(60.0);

    // PTT hook — fires callback on T or U press/release
    PTTListener ptt;
    ptt.start([&mic](bool active) {
        mic.setPTT(active);
        cout << "[PTT] " << (active ? "ON" : "OFF") << "\n";
    });

    RollingBufferManager videoBuffer(60.0);
    HotkeyListener hotkey;
    if (!hotkey.initialize()) { return -1; }

    cout << "[Main] Capture started, waiting for frames...\n";

    auto lastFrameTime = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point windowStartWallClock =
        std::chrono::steady_clock::now();

    // Low-level hook needs a message pump to fire
    // We already have one in hotkey.poll() via PeekMessage
    while (true) {
        if (hotkey.poll()) {
            cout << "[Hotkey] pressed! Saving Last 60 seconds\n";

            std::string ts = getTimestamp();
            std::string tempVideo = "temp_"  + ts + ".h264";
            std::string tempAudio = "audio_" + ts + ".wav";
            std::string tempMic   = "mic_"   + ts + ".wav";
            std::string finalClip = "clip_"  + ts + ".mp4";

            auto videoshot = videoBuffer.snapshot();
            videoBuffer.writeToFile(videoshot, tempVideo);

            auto now2 = std::chrono::steady_clock::now();
            double windowAge = std::chrono::duration<double>(
                now2 - windowStartWallClock).count();
            cout << "[Main] windowStartWallClock age = " << windowAge << "s\n";

            audio.saveSnapshot(tempAudio, windowStartWallClock);
            mic.saveSnapshot(tempMic, windowStartWallClock);

            if (FFmpegMuxer::mux(tempVideo, tempAudio, tempMic, finalClip)) {
                cout << "[Hotkey] Clip Saved: " << finalClip << "\n";
                std::filesystem::remove(tempVideo);
                std::filesystem::remove(tempAudio);
                std::filesystem::remove(tempMic);
            } else {
                cout << "[Muxer] Failed!\n";
            }
        }

        if (cap.hasNewFrame()) {
            ID3D11Texture2D* frame = cap.getFrame();
            if (frame) {
                auto now = std::chrono::steady_clock::now();
                double realElapsed = std::chrono::duration<double>(
                    now - lastFrameTime).count();
                lastFrameTime = now;

                if (realElapsed > 0.5) realElapsed = 1.0 / 60.0;

                vector<uint8_t> h264;
                bool isKeyframe = false;
                if (encoder->encodeFrame(frame, h264, isKeyframe) && !h264.empty()) {
                    EncodedFragment frag;
                    frag.data        = h264;
                    frag.durationSec = realElapsed;
                    frag.isIDR       = isKeyframe;
                    videoBuffer.pushFragment(frag);

                    double bufDuration = videoBuffer.currentDuration();
                    auto bufDur = std::chrono::duration_cast<
                        std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(bufDuration));
                    windowStartWallClock = now - bufDur;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    ptt.stop();
    encoder->shutdown();
    cout << "Done\n";
    return 0;
}