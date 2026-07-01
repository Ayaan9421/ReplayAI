#include "FFmpegAudioRecorder.h"

#include <filesystem>
#include <iostream>
#include <vector>

using namespace std;

FFmpegAudioRecorder::FFmpegAudioRecorder() { ZeroMemory(&m_proc, sizeof(m_proc)); }

FFmpegAudioRecorder::~FFmpegAudioRecorder() { stop(); }

bool FFmpegAudioRecorder::start(const string& deviceName, const string& outPath) {
    if (m_running) return true;

    m_audioPath = outPath;

    // Pipe for stdin
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdinRead = nullptr;
    if (!CreatePipe(&stdinRead, &m_stdinWrite, &sa, 0)) {
        cout << "[FFmpegAudioRecorder] CreatePipe failed\n";
        return false;
    }

    SetHandleInformation(m_stdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    string cmd =
        "ffmpeg -y "
        "-f dshow "
        "-i audio=\"" +
        deviceName +
        "\" "
        "-ac 2 -ar 48000 "
        "\"" +
        outPath + "\"";

    vector<char> cmdBuff(cmd.begin(), cmd.end());
    cmdBuff.push_back('\0');

    cout << cmd << endl;
    BOOL ok = CreateProcessA(nullptr, cmdBuff.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si,
                             &m_proc);

    CloseHandle(stdinRead);

    if (!ok) {
        cout << "[FFmpegAudioRecorder] Failed to start FFmpeg (err = \n" << GetLastError() << ")" << endl;
        CloseHandle(m_stdinWrite);
        m_stdinWrite = nullptr;
        return false;
    }

    m_running = true;

    cout << "[FFmpegAudioRecorder] Recording audio from " << deviceName << endl;
    return true;
}

bool FFmpegAudioRecorder::finalizeClip(const string& h264Path, const string& outMp4,
                                        const string& audioPath, int fps) {
    const string tsPath = "temp.ts";
    if (!convertH264ToTS(h264Path, tsPath, fps)) return false;
    if (!muxTSWithAudio(tsPath, audioPath, outMp4)) return false;  // use passed-in path
    cout << "[Pipeline] Final clip created => " << outMp4 << endl;
    return true;
}

void FFmpegAudioRecorder::stop() {
    if (!m_running) return;

    DWORD written = 0;
    WriteFile(m_stdinWrite, "q\n", 2, &written, nullptr);
    CloseHandle(m_stdinWrite);
    m_stdinWrite = nullptr;

    DWORD waitResult = WaitForSingleObject(m_proc.hProcess, 5000);
    if (waitResult == WAIT_TIMEOUT) {
        cout << "[FFmpegAudioRecorder] WARNING: FFmpeg did not exit gracefully, force killing\n";
        TerminateProcess(m_proc.hProcess, 1);
        WaitForSingleObject(m_proc.hProcess, 2000); // wait for termination
    }

    CloseHandle(m_proc.hProcess);
    CloseHandle(m_proc.hThread);
    ZeroMemory(&m_proc, sizeof(m_proc));

    m_running = false;

    Sleep(200);


    if (!filesystem::exists(m_audioPath)) {
        cout << "[FFmpegAudioRecorder] ERROR: audio file not created\n";
    } else {
        cout << "[FFmpegAudioRecorder] Audio saved => " << m_audioPath << endl;
    }
}

bool FFmpegAudioRecorder::convertH264ToTS(const string& h264Path, const string& tsPath, int fps) {
    string cmd =
        "ffmpeg -y "
        "-fflags +genpts "
        "-r " +
        to_string(fps) + " -i \"" + h264Path +
        "\" "
        "-c copy "
        "-bsf:v h264_mp4toannexb "
        "-f mpegts "
        "\"" +
        tsPath + "\"";

    cout << "[FFmpeg] " << cmd << endl;
    return system(cmd.c_str()) == 0;
}

bool FFmpegAudioRecorder::muxTSWithAudio(const string& tsPath, const string& audioPath, const string& outMp4) {
    string cmd =
        "ffmpeg -y "
        "-i \"" +
        tsPath +
        "\" "
        "-i \"" +
        audioPath +
        "\" "
        "-map 0:v:0 -map 1:a:0 "
        "-c:v copy "
        "-c:a aac -b:a 192k "
        "-movflags +faststart "
        "-shortest "
        "\"" +
        outMp4 + "\"";

    cout << "[FFmpeg] " << cmd << endl;
    return system(cmd.c_str()) == 0;
}