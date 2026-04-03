#include "FFmpegMuxer.h"

#include <cstdlib>
#include <iostream>

using namespace std;

bool FFmpegMuxer::mux(const string& h264Path, const string& wavPath, const string& outMp4, int fps) {
    const string tsPath = "temp.ts";

    // Step 1: h264 -> ts
    string cmd1 =
        "ffmpeg -y "
        "-fflags +genpts "
        "-r " + to_string(fps) + " "
        "-i \"" + h264Path + "\" "
        "-c copy "
        "-bsf:v h264_mp4toannexb "
        "-f mpegts "
        "\"" + tsPath + "\"";

    cout << "[FFmpegMuxer] " << cmd1 << endl;
    if (system(cmd1.c_str()) != 0) {
        cout << "[FFmpegMuxer] ERROR: h264->ts failed\n";
        return false;
    }

    double videoDuration  = probeDuration(tsPath);
    double audioDuration  = probeDuration(wavPath);
    double videoStartTime = probeStartTime(tsPath);   // gets the 1.4s
    double audioSkip      = (audioDuration - videoDuration) + videoStartTime;
    if (audioSkip < 0) audioSkip = 0;

    cout << "[FFmpegMuxer] Video=" << videoDuration 
         << "s  Audio=" << audioDuration 
         << "s  Skipping first " << audioSkip << "s of audio\n";

    
    char skipBuf[32];
    snprintf(skipBuf, sizeof(skipBuf), "%.3f", audioSkip);

    string cmd2 =
        "ffmpeg -y "
        "-i \"" + tsPath + "\" "
        "-ss " + string(skipBuf) + " "
        "-f wav -i \"" + wavPath + "\" "
        "-map 0:v:0 -map 1:a:0 "
        "-c:v copy "
        "-c:a aac -b:a 192k "
        "-movflags +faststart "
        "-shortest "
        "\"" + outMp4 + "\"";

    cout << "[FFmpegMuxer] " << cmd2 << endl;
    if (system(cmd2.c_str()) != 0) {
        cout << "[FFmpegMuxer] ERROR: mux failed\n";
        return false;
    }

    cout << "[FFmpegMuxer] Clip created => " << outMp4 << endl;
    return true;
}

double FFmpegMuxer::probeDuration(const string& path) {
    string cmd = "ffprobe -v error -show_entries format=duration "
                 "-of csv=p=0 \"" + path + "\"";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return 0.0;
    char buf[64] = {};
    fgets(buf, sizeof(buf), pipe);
    _pclose(pipe);
    return atof(buf);
}

double FFmpegMuxer::probeStartTime(const string& path) {
    string cmd = "ffprobe -v error -show_entries stream=start_time "
                 "-select_streams v:0 -of csv=p=0 \"" + path + "\"";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return 0.0;
    char buf[64] = {};
    fgets(buf, sizeof(buf), pipe);
    _pclose(pipe);
    double val = atof(buf);
    return (val > 0.0 && val < 10.0) ? val : 0.0;
}
