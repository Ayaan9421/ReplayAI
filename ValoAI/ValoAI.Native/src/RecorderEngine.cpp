#include "RecorderEngine.h"

#include <winrt/windows.foundation.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <mutex>

#include "capture/ScreenCaptureWGC.h"
#include "core/D3DContext.h"
#include "encoder/NVEncoder.h"
#include "buffer/RollingBufferManager.h"
#include "audio/WASAPILoopbackRecorder.h"
#include "audio/MicRollingBuffer.h"
#include "utils/FFmpegMuxer.h"
#include "utils/PTTListener.h"

using namespace std;
using namespace std::chrono;



struct RecorderEngine::Impl {
    D3DContext              d3d;
    ScreenCaptureWGC        cap;
    unique_ptr<NVEncoder>   encoder;
    RollingBufferManager* videoBuffer = nullptr;
    WASAPILoopbackRecorder* audio = nullptr;
    MicRollingBuffer* mic = nullptr;
    PTTListener* ptt = nullptr;

    EngineSettings settings;

    steady_clock::time_point windowStartWallClock;
    steady_clock::time_point lastFrameTime;
    bool running = false;
};

RecorderEngine::RecorderEngine() {}
RecorderEngine::~RecorderEngine() { stop(); }


static bool RunLoggedCommand(const std::string& cmd, std::string& outLog) {
    SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi{};
    std::string full = "cmd.exe /C " + cmd;
    std::vector<char> buf(full.begin(), full.end());
    buf.push_back('\0');

    BOOL ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) { CloseHandle(hRead); return false; }

    char chunk[4096]; DWORD n; std::ostringstream log;
    while (ReadFile(hRead, chunk, sizeof(chunk) - 1, &n, nullptr) && n > 0) {
        chunk[n] = '\0';
        log << chunk;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(hRead); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    outLog = log.str();
    return exitCode == 0;
}

bool RecorderEngine::start(const EngineSettings& settings) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    auto* impl = new Impl();
    impl->settings = settings;

    if (!impl->d3d.initialize()) { delete impl; return false; }
    if (!impl->cap.initialize(&impl->d3d)) { delete impl; return false; }

    impl->encoder = make_unique<NVEncoder>();
    if (!impl->encoder->initialize(impl->d3d.device(), 1920, 1080)) {
        delete impl; return false;
    }

    impl->videoBuffer = new RollingBufferManager(settings.bufferDurationSec);
    impl->audio = new WASAPILoopbackRecorder();
    impl->audio->start(settings.bufferDurationSec);

    if (settings.micEnabled) {
        impl->mic = new MicRollingBuffer();
        impl->mic->start(settings.bufferDurationSec);
    }

    impl->ptt = new PTTListener();
    impl->ptt->start([impl](bool active) {
        if (impl->mic) impl->mic->setPTT(active);
        });

    impl->lastFrameTime = steady_clock::now();
    impl->windowStartWallClock = steady_clock::now();
    impl->running = true;

    m_impl = impl;
    m_running = true;
    return true;
}

void RecorderEngine::tick() {
    if (!m_impl || !m_running) return;
    auto* impl = m_impl;

    try {
        bool hasFrame = impl->cap.hasNewFrame();

        if (hasFrame) {
            auto* frame = impl->cap.getFrame();
            if (frame) {
                auto now = steady_clock::now();
                double elapsed = duration<double>(now - impl->lastFrameTime).count();
                impl->lastFrameTime = now;
                if (elapsed > 0.5) elapsed = 1.0 / 60.0;

                vector<uint8_t> h264;
                bool isKey = false;
                if (impl->encoder->encodeFrame(frame, h264, isKey) && !h264.empty()) {
                    EncodedFragment frag;
                    frag.data = h264;
                    frag.durationSec = elapsed;
                    frag.isIDR = isKey;
                    impl->videoBuffer->pushFragment(frag);

                    double bufDur = impl->videoBuffer->currentDuration();
                    auto   bd = duration_cast<steady_clock::duration>(
                        duration<double>(bufDur));
                    impl->windowStartWallClock = now - bd;
                }
            }
        }

    }
    catch (const winrt::hresult_error& ex) {
        static bool logged = false;
        if (!logged) {
            std::wstring wmsg = ex.message().c_str();
            std::string msg(wmsg.begin(), wmsg.end());
            logged = true;
        }
    }   
}

bool RecorderEngine::saveClip(ClipResult& outResult) {
    if (!m_impl || !m_running) {
        return false;
    }
    auto* impl = m_impl;

    auto   now = system_clock::now();
    auto   t = system_clock::to_time_t(now);
    tm     tm{};
    localtime_s(&tm, &t);
    char   buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    string ts(buf);

    string& folder = impl->settings.clipsFolder;
    string tempVideo = folder + "\\temp_" + ts + ".h264";
    string tempAudio = folder + "\\audio_" + ts + ".wav";
    string tempMic = folder + "\\mic_" + ts + ".wav";
    string finalClip = folder + "\\clip_" + ts + ".mp4";
    string thumbPath = folder + "\\thumb_" + ts + ".jpg";

    auto shot = impl->videoBuffer->snapshot();
    impl->videoBuffer->writeToFile(shot, tempVideo);
    impl->audio->saveSnapshot(tempAudio, impl->windowStartWallClock);

    if (impl->mic)
        impl->mic->saveSnapshot(tempMic, impl->windowStartWallClock);

    auto videoSize = filesystem::exists(tempVideo) ? filesystem::file_size(tempVideo) : 0;
    auto audioSize = filesystem::exists(tempAudio) ? filesystem::file_size(tempAudio) : 0;

    bool ok;
    if (impl->mic)
        ok = FFmpegMuxer::mux(tempVideo, tempAudio, tempMic, finalClip);
    else
        ok = FFmpegMuxer::mux(tempVideo, tempAudio, "", finalClip);

    filesystem::remove(tempVideo);
    filesystem::remove(tempAudio);
    if (impl->mic) filesystem::remove(tempMic);

    if (ok) {
        string thumbPath = folder + "\\thumb_" + ts + ".jpg";
        string thumbCmd = "ffmpeg -y -i \"" + finalClip + "\" -ss 00:00:01 -vframes 1 \"" + thumbPath + "\"";
        string log;
        RunLoggedCommand(thumbCmd, log);
        
        outResult.filePath = finalClip;
        outResult.thumbnailPath = thumbPath;
        outResult.durationSec = 60.0;
    }
    return ok;
}

void RecorderEngine::stop() {
    if (!m_impl) return;
    auto* impl = m_impl;
    impl->running = false;

    if (impl->ptt) { impl->ptt->stop();   delete impl->ptt; }
    if (impl->mic) { impl->mic->stop();   delete impl->mic; }
    if (impl->audio) { impl->audio->stop(); delete impl->audio; }
    if (impl->videoBuffer) { delete impl->videoBuffer; }
    if (impl->encoder) { impl->encoder->shutdown(); }

    delete impl;
    m_impl = nullptr;
    m_running = false;
}