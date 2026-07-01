#include "RecorderService.h"
#include "RecorderEngine.h"
#include <string>
#include <thread>
#include <atomic>

using namespace System;
using namespace System::IO;
using namespace System::Threading;
using namespace std;
using namespace System::Collections::Generic;
using namespace System::Diagnostics;

// Manual string conversion — avoids marshal header conflicts
static std::string ToStdString(System::String^ s) {
    using namespace System::Runtime::InteropServices;
    const char* chars = (const char*)(Marshal::StringToHGlobalAnsi(s)).ToPointer();
    std::string result(chars);
    Marshal::FreeHGlobal(System::IntPtr((void*)chars));
    return result;
}

namespace ValoAI {
    namespace Core {

        // Holds the native engine + capture thread
        struct NativeBridge {
            RecorderEngine          engine;
            thread                  captureThread;
            atomic<bool>            threadRunning{ false };
        };

        RecorderService::RecorderService() {
            m_initialized = false;
            m_nativeRecorder = nullptr;
        }

        RecorderService::~RecorderService() { this->!RecorderService(); }
        RecorderService::!RecorderService() { Shutdown(); }

        static void CaptureThreadProc(NativeBridge* bridge)
        {
            while (bridge->threadRunning)
            {
                bridge->engine.tick();
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        }

        bool RecorderService::Initialize(RecorderSettings^ settings) {
            m_settings = settings;

            if (!Directory::Exists(settings->ClipsFolder))
                Directory::CreateDirectory(settings->ClipsFolder);

            EngineSettings es;
            es.clipsFolder = ToStdString(settings->ClipsFolder);
            es.bufferDurationSec = settings->BufferDurationSec;
            es.targetFps = settings->TargetFps;
            es.pttKey1 = settings->PTTKey1VK;
            es.pttKey2 = settings->PTTKey2VK;
            es.micEnabled = settings->MicEnabled;

            auto* bridge = new NativeBridge();

            if (!bridge->engine.start(es)) {
                delete bridge;
                StatusChanged("Engine start failed");
                return false;
            }

            // Start capture thread
            bridge->threadRunning = true;
            bridge->captureThread = std::thread(CaptureThreadProc, bridge);

            m_nativeRecorder = bridge;
            m_initialized = true;
            StatusChanged("Recorder ready");
            return true;
        }

        bool RecorderService::SaveClip() {
            Debug::WriteLine("[ValoAI-CLI] RecorderService::SaveClip() entered\n");


            if (!m_initialized) {
                Debug::WriteLine("[ValoAI-CLI] m_initialized is FALSE, bailing out\n");
                return false;
            }
            auto* bridge = static_cast<NativeBridge*>(m_nativeRecorder);
            Debug::WriteLine("[ValoAI-CLI] calling bridge->engine.saveClip()\n");

            ClipResult result;
            bool ok = bridge->engine.saveClip(result);
            Debug::WriteLine(String::Format("[ValoAI-CLI] saveClip returned ok={0}", ok));

            if (ok) {
                auto info = gcnew ClipInfo();
                info->FilePath = gcnew String(result.filePath.c_str());
                info->ThumbnailPath = gcnew String(result.thumbnailPath.c_str());
                info->CreatedAt = DateTime::Now;
                info->DurationSec = result.durationSec;
                ClipSaved(info);
                StatusChanged("Clip saved: " + info->FilePath);
            }
            return ok;
        }

        List<ClipInfo^>^ RecorderService::GetClips() {
            auto list = gcnew List<ClipInfo^>();
            if (!Directory::Exists(m_settings->ClipsFolder)) return list;

            auto files = Directory::GetFiles(m_settings->ClipsFolder, "clip_*.mp4");
            Array::Sort(files);

            for each (String ^ f in files) {
                auto info = gcnew ClipInfo();
                info->FilePath = f;
                info->CreatedAt = File::GetCreationTime(f);
                String^ thumb = f->Replace("clip_", "thumb_")->Replace(".mp4", ".jpg");
                info->ThumbnailPath = File::Exists(thumb) ? thumb : nullptr;
                list->Add(info);
            }
            return list;
        }

        void RecorderService::Shutdown() {
            if (!m_initialized) return;
            auto* bridge = static_cast<NativeBridge*>(m_nativeRecorder);

            bridge->threadRunning = false;
            if (bridge->captureThread.joinable())
                bridge->captureThread.join();

            bridge->engine.stop();
            delete bridge;

            m_nativeRecorder = nullptr;
            m_initialized = false;
        }

    }
}