#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct EncodedFragment {
    std::vector<uint8_t> data;
    double durationSec;
};

class RollingBufferManager {
   public:
    explicit RollingBufferManager(double maxDurationSec);

    // push encoded fragment(h264 bytes)
    void pushFragment(const std::vector<uint8_t>& encodedData, double durationSec);

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
