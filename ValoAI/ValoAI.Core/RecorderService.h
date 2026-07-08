#pragma once

#include <vcclr.h>

using namespace System;
using namespace System::Collections::Generic;

namespace ValoAI {
    namespace Core {

        public ref class RecorderSettings {
        public:
            property String^ ClipsFolder;
            property int     BufferDurationSec;
            property int     TargetFps;
            property int     ClipHotkeyVK;
            property int     ClipHotkeyMods;
            property int     PTTKey1VK;
            property int     PTTKey2VK;
            property bool    MicEnabled;
            property int     Width;
            property int     Height;
            property int     BitrateMbps;
            property String^ MicDeviceName;
            property int     DisplayIndex;


            RecorderSettings() {
                ClipsFolder = Environment::GetFolderPath(
                    Environment::SpecialFolder::MyDocuments)
                    + "\\ValoAI\\Clips";
                BufferDurationSec = 60;
                TargetFps = 60;
                ClipHotkeyVK = 0x78;
                ClipHotkeyMods = 0x06;
                PTTKey1VK = 0x54;
                PTTKey2VK = 0x55;
                MicEnabled = true;
            }
        };

        public ref class ClipInfo {
        public:
            property String^ FilePath;
            property String^ ThumbnailPath;
            property DateTime CreatedAt;
            property double   DurationSec;
        };

        public ref class RecorderService {
        public:
            RecorderService();
            ~RecorderService();
            !RecorderService();

            bool             Initialize(RecorderSettings^ settings);
            void             Shutdown();
            bool             SaveClip();
            List<ClipInfo^>^ GetClips();

            event Action<ClipInfo^>^ ClipSaved;
            event Action<String^>^ StatusChanged;

        private:
            RecorderSettings^ m_settings;
            bool              m_initialized;
            void* m_nativeRecorder;
        };

    }
}