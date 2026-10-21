#pragma once
#include "backend_pool.h"
#include "health_checker.h"
#include <string>
#include <atomic>
#include <vector>

struct LBConfig {
    int         port            = 8080;
    int         health_interval = 5;
    int         health_timeout  = 2;
    LBAlgorithm algorithm       = LBAlgorithm::ROUND_ROBIN;
    struct BackendEntry { std::string host; int port; };
    std::vector<BackendEntry> backends;
};

class LoadBalancer {
public:
    explicit LoadBalancer(const LBConfig& config);
    ~LoadBalancer();
    void run();
    void stop();

private:
    void acceptLoop();

    LBConfig          config_;
    BackendPool       pool_;
    HealthChecker     health_checker_;
    std::atomic<bool> running_{false};

    // Store as uintptr_t to avoid pulling winsock into the header
    uintptr_t server_fd_{(uintptr_t)-1};
};
