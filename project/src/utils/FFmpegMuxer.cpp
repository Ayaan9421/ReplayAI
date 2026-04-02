#include "FFmpegMuxer.h"

#include <cstdlib>
#include <iostream>

using namespace std;

bool FFmpegMuxer::mux(const string& h264Path, const string& wavPath, const string& outMp4, int fps) {
    const string tsPath = "temp.ts";

    // Step 1: h264 -> ts (this assigns proper timestamps)
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

    // Step 2: ts + wav -> mp4 (timestamps are now valid, -shortest works correctly)
    string cmd2 =
        "ffmpeg -y "
        "-i \"" + tsPath + "\" "
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