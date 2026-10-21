#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

enum class BackendStatus { HEALTHY, UNHEALTHY };
enum class LBAlgorithm  { ROUND_ROBIN, LEAST_CONNECTIONS };

struct Backend {
    std::string host;
    int         port;
    BackendStatus status;
    std::atomic<int> active_connections{0};
    std::atomic<int> failure_count{0};
    std::atomic<int> success_count{0};

    std::atomic<long long> latency_ms{0};

    Backend(const std::string& h, int p)
        : host(h), port(p), status(BackendStatus::HEALTHY) {}

    Backend(const Backend& o)
        : host(o.host), port(o.port), status(o.status),
          active_connections(o.active_connections.load()),
          failure_count(o.failure_count.load()),
          success_count(o.success_count.load()),
          latency_ms(o.latency_ms.load()) {}

    Backend& operator=(const Backend& o) {
        host = o.host; port = o.port; status = o.status;
        active_connections.store(o.active_connections.load());
        failure_count.store(o.failure_count.load());
        success_count.store(o.success_count.load());
        latency_ms.store(o.latency_ms.load());
        return *this;
    }
};

class BackendPool {
public:
    BackendPool();
    void addBackend(const std::string& host, int port);
    int  selectBackend();
    void markHealthy(int index);
    void markUnhealthy(int index);
    void incrementConnections(int index);
    void decrementConnections(int index);
    void recordHealthSuccess(int index,long long latency);
    void recordHealthFailure(int index);
    std::vector<Backend> getAll() const;
    int  size() const;
    void setAlgorithm(LBAlgorithm algo);
    LBAlgorithm getAlgorithm() const;

private:
    int roundRobinSelect();
    int leastConnectionsSelect();

    // Use plain mutex (shared_mutex not reliable on older MinGW)
    mutable std::mutex   mutex_;
    std::vector<Backend> backends_;
    std::atomic<int>     rr_counter_{0};
    LBAlgorithm          algorithm_{LBAlgorithm::ROUND_ROBIN};
};
