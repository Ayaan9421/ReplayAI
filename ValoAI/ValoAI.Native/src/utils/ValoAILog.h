#pragma once
#include <string>
#include <fstream>
#include <mutex>

inline std::mutex g_logMutex;

inline void FileLog(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream f(
        "C:\\Users\\Admin\\Downloads\\temp\\ValoAI_debug.log",
        std::ios::app);
    if (f) f << msg;
}