#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

struct EncodedFragment {
    std::vector<uint8_t> data;
    double durationSec;
    bool isIDR = false;  // true if this fragment starts with an IDR keyframe
};

class RollingBufferManager {
   public:
    explicit RollingBufferManager(double maxDurationSec);

    // push encoded fragment(h264 bytes)
    void pushFragment(const EncodedFragment& frag);
    // snapshot buffer safely
    std::deque<EncodedFragment> snapshot() const;

    // dump last N seconds into a single file
    bool writeToFile(const std::deque<EncodedFragment>& snapshot, const std::string& filePath) const;

    // clear everything
    void reset();

    double currentDuration() const;

   private:
    void trimIfNeeded();

   private:
    double m_maxDurationSec;
    double m_currentDurationSec = 0.0;

    std::deque<EncodedFragment> m_queue;
};
