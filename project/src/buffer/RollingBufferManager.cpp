#include "RollingBufferManager.h"

#include <fstream>
#include <iostream>

using namespace std;

RollingBufferManager::RollingBufferManager(double maxDurationSec) : m_maxDurationSec(maxDurationSec) {}

void RollingBufferManager::pushFragment(const vector<uint8_t>& encodedData, double durationSec) {
    EncodedFragment frag;
    frag.data = encodedData;
    frag.durationSec = durationSec;

    m_queue.push_back(std::move(frag));
    m_currentDurationSec += durationSec;

    trimIfNeeded();
}

void RollingBufferManager::trimIfNeeded() {
    while (m_currentDurationSec > m_maxDurationSec && !m_queue.empty()) {
        m_currentDurationSec -= m_queue.front().durationSec;
        m_queue.pop_front();
    }
}

bool RollingBufferManager::writeToFile(const std::deque<EncodedFragment>& snapshot, const string& filePath) const {
    if (snapshot.empty()) {
        cout << "[RollingBufferManager] Snapshot empty. No data to write" << endl;
        return false;
    }

    ofstream ofs(filePath, ios::binary);

    if (!ofs) {
        cout << "[RollingBufferManager] Failed to open output file" << endl;
        return false;
    }

    for (const auto& frag : snapshot) ofs.write(reinterpret_cast<const char*>(frag.data.data()), frag.data.size());

    ofs.close();

    cout << "[RollingBufferManager] Saved " << snapshot.size() << " fragments to " << filePath << endl;

    return true;
}

deque<EncodedFragment> RollingBufferManager::snapshot() const { return m_queue; }

void RollingBufferManager::reset() {
    m_queue.clear();
    m_currentDurationSec = 0.0;
}

double RollingBufferManager::currentDuration() const { return m_currentDurationSec; }
