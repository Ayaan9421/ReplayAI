// #include "FFmpegMuxer.h"

// #include <cstdlib>
// #include <iostream>

// using namespace std;

// bool FFmpegMuxer::mux(const string& h264Path, const string& wavPath, const string& outMp4, int fps) {
//     const string tsPath = "temp.ts";

//     // Step 1: h264 -> ts
//     string cmd1 =
//         "ffmpeg -y "
//         "-fflags +genpts "
//         "-r " + to_string(fps) + " "
//         "-i \"" + h264Path + "\" "
//         "-c copy "
//         "-bsf:v h264_mp4toannexb "
//         "-f mpegts "
//         "\"" + tsPath + "\"";

//     cout << "[FFmpegMuxer] " << cmd1 << endl;
//     if (system(cmd1.c_str()) != 0) {
//         cout << "[FFmpegMuxer] ERROR: h264->ts failed\n";
//         return false;
//     }

//     // No -ss on video at all
//     string cmd2 =
//         "ffmpeg -y "
//         "-i \"" + tsPath + "\" "
//         "-f wav -i \"" + wavPath + "\" "
//         "-map 0:v:0 -map 1:a:0 "
//         "-c:v copy "
//         "-c:a aac -b:a 192k "
//         "-movflags +faststart "
//         "-shortest "
//         "\"" + outMp4 + "\"";
        
//     cout << "[FFmpegMuxer] " << cmd2 << endl;
//     if (system(cmd2.c_str()) != 0) {
//         cout << "[FFmpegMuxer] ERROR: mux failed\n";
//         return false;
//     }

//     cout << "[FFmpegMuxer] Clip created => " << outMp4 << endl;
//     return true;
// }

#include "FFmpegMuxer.h"
#include <cstdlib>
#include <iostream>
using namespace std;

bool FFmpegMuxer::mux(const string& h264Path, const string& wavPath,
                      const string& micPath, const string& outMp4, int fps) {
    const string tsPath    = "temp.ts";
    const string noMicClip = "temp_nomicclip.mp4";

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

    // Step 2: mux video + game audio (the proven working pipeline)
    string cmd2 =
        "ffmpeg -y "
        "-i \"" + tsPath + "\" "
        "-f wav -i \"" + wavPath + "\" "
        "-map 0:v:0 -map 1:a:0 "
        "-c:v copy "
        "-c:a aac -b:a 192k "
        "-movflags +faststart "
        "-shortest "
        "\"" + noMicClip + "\"";

    cout << "[FFmpegMuxer] " << cmd2 << endl;
    if (system(cmd2.c_str()) != 0) {
        cout << "[FFmpegMuxer] ERROR: video+audio mux failed\n";
        return false;
    }

    // Step 3: mix mic into the already-synced clip
    // noMicClip has correct duration and sync
    // mic WAV is same duration as game audio, so we just mix them
    // amerge combines the existing AAC track with mic, then re-encodes
    string cmd3 =
        "ffmpeg -y "
        "-i \"" + noMicClip + "\" "
        "-f wav -i \"" + micPath + "\" "
        "-filter_complex "
        "\"[0:a][1:a]amix=inputs=2:duration=first:dropout_transition=0[aout]\" "
        "-map 0:v:0 "
        "-map \"[aout]\" "
        "-c:v copy "
        "-c:a aac -b:a 192k "
        "-movflags +faststart "
        "\"" + outMp4 + "\"";

    cout << "[FFmpegMuxer] " << cmd3 << endl;
    if (system(cmd3.c_str()) != 0) {
        cout << "[FFmpegMuxer] ERROR: mic mix failed\n";
        return false;
    }

    std::remove(tsPath.c_str());
    std::remove(noMicClip.c_str());
    cout << "[FFmpegMuxer] Clip created => " << outMp4 << endl;
    return true;
}