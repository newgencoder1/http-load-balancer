#pragma once
#include "backend_pool.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <string>

class HealthChecker {
public:
    explicit HealthChecker(BackendPool& pool,
                           int interval_s = 5,
                           int timeout_s  = 2);
    ~HealthChecker();
    void start();
    void stop();

private:
    void run();
    bool pingBackend(const std::string& host, int port);

    BackendPool&      pool_;
    int               interval_s_;
    int               timeout_s_;
    std::atomic<bool> running_{false};
    std::thread       thread_;
};
