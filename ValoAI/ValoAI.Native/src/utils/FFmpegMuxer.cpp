#include "FFmpegMuxer.h"
#include <cstdlib>
#include <iostream>
#include <Windows.h>
#include <vector>
#include <sstream>
#include <mutex>
#include <fstream>

using namespace std;

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
    string log;
    if (!RunLoggedCommand(cmd1.c_str(), log)) {
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
    if (!RunLoggedCommand(cmd2.c_str(), log)) {
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


    if (!RunLoggedCommand(cmd3.c_str(), log)) {
        cout << "[FFmpegMuxer] ERROR: video+audio mux failed\n";
        return false;
    }

    std::remove(tsPath.c_str());
    std::remove(noMicClip.c_str());
    cout << "[FFmpegMuxer] Clip created => " << outMp4 << endl;
    return true;
}