#pragma once

#include <windows.h>

#include <string>

class FFmpegAudioRecorder {
   public:
    FFmpegAudioRecorder();
    ~FFmpegAudioRecorder();

    bool start(const std::string& dceviceName, const std::string& outAudioPath);
    void stop();

    // FFmpegAudioRecorder.h - change the signature
bool finalizeClip(const std::string& h264Path, const std::string& outMp4, 
                  const std::string& audioPath, int fps = 60);

   private:
    bool convertH264ToTS(const std::string& h264Path, const std::string& tsPath, int fps = 60);
    bool muxTSWithAudio(const std::string& tsPath, const std::string& audioPath, const std::string& outMp4);

    PROCESS_INFORMATION m_proc{};
    HANDLE m_stdinWrite = nullptr;
    bool m_running = false;
    std::string m_audioPath;
};