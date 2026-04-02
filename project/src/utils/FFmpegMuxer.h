#pragma once
#include <string>

class FFmpegMuxer {
   public:
    static bool mux(const std::string& h264Path, const std::string& wavPath, const std::string& outMp4, int fps = 60);
};
