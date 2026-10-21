#include "backend_pool.h"
#include <climits>
#include <mutex>

BackendPool::BackendPool() = default;

void BackendPool::addBackend(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    backends_.emplace_back(host, port);
}

int BackendPool::selectBackend() {
    switch (algorithm_) {
        case LBAlgorithm::ROUND_ROBIN:       return roundRobinSelect();
        case LBAlgorithm::LEAST_CONNECTIONS: return leastConnectionsSelect();
        default: return roundRobinSelect();
    }
}

int BackendPool::roundRobinSelect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (backends_.empty()) return -1;
    int n = (int)backends_.size();
    for (int attempt = 0; attempt < n; ++attempt) {
        int idx = rr_counter_.fetch_add(1) % n;
        if (backends_[idx].status == BackendStatus::HEALTHY)
            return idx;
    }
    return -1;
}

int BackendPool::leastConnectionsSelect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (backends_.empty()) return -1;
    int best_idx = -1, best_conn = INT_MAX;
    for (int i = 0; i < (int)backends_.size(); ++i) {
        if (backends_[i].status == BackendStatus::HEALTHY) {
            int c = backends_[i].active_connections.load();
            if (c < best_conn) { best_conn = c; best_idx = i; }
        }
    }
    return best_idx;
}

void BackendPool::markHealthy(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= 0 && index < (int)backends_.size())
        backends_[index].status = BackendStatus::HEALTHY;
}

void BackendPool::markUnhealthy(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= 0 && index < (int)backends_.size())
        backends_[index].status = BackendStatus::UNHEALTHY;
}

void BackendPool::incrementConnections(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= 0 && index < (int)backends_.size())
        backends_[index].active_connections.fetch_add(1);
}

void BackendPool::decrementConnections(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= 0 && index < (int)backends_.size()) {
        int cur = backends_[index].active_connections.load();
        if (cur > 0) backends_[index].active_connections.fetch_sub(1);
    }
}

void BackendPool::recordHealthSuccess(
    int index,
    long long latency)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if(index < 0 || index >= backends_.size())
        return;

    auto& b = backends_[index];

    b.success_count++;
    b.failure_count = 0;

    b.latency_ms = latency;

    if(b.success_count >= 2)
        b.status = BackendStatus::HEALTHY;
}

void BackendPool::recordHealthFailure(int index)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if(index < 0 || index >= backends_.size())
        return;

    auto& b = backends_[index];

    b.failure_count++;
    b.success_count = 0;

    if(b.failure_count >= 3)
        b.status = BackendStatus::UNHEALTHY;
}

std::vector<Backend> BackendPool::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return backends_;
}

int BackendPool::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (int)backends_.size();
}

void BackendPool::setAlgorithm(LBAlgorithm algo) { algorithm_ = algo; }
LBAlgorithm BackendPool::getAlgorithm() const    { return algorithm_; }
