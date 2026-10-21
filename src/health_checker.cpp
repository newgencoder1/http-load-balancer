#include "health_checker.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define CLOSE_SOCKET(s) closesocket(s)
  #define INVALID_SOCKET_VAL INVALID_SOCKET
  typedef SOCKET SockFd;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #define CLOSE_SOCKET(s) close(s)
  #define INVALID_SOCKET_VAL (-1)
  typedef int SockFd;
#endif

HealthChecker::HealthChecker(BackendPool& pool, int interval_s, int timeout_s)
    : pool_(pool), interval_s_(interval_s), timeout_s_(timeout_s) {}

HealthChecker::~HealthChecker() { stop(); }

void HealthChecker::start() {
    running_ = true;
    thread_ = std::thread(&HealthChecker::run, this);
    std::cout << "[HealthChecker] Started (interval=" << interval_s_
              << "s, timeout=" << timeout_s_ << "s)\n";
}

void HealthChecker::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void HealthChecker::run() {
    while (running_) {
        auto backends = pool_.getAll();
        for (int i = 0; i < (int)backends.size(); ++i) {
            auto start = std::chrono::steady_clock::now();
            bool alive = pingBackend(backends[i].host, backends[i].port);
            auto end = std::chrono::steady_clock::now();

            long long latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                end-start).count();

            if(alive) {
                pool_.recordHealthSuccess(i, latency);
            }
            else {
                pool_.recordHealthFailure(i);
            }
        }
        for (int s = 0; s < interval_s_ && running_; ++s)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "[HealthChecker] Stopped.\n";
}

bool HealthChecker::pingBackend(const std::string& host, int port) {
    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(port);

    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
        return false;

    int raw_sock = (int)socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (raw_sock < 0) { freeaddrinfo(res); return false; }

#ifdef _WIN32
    DWORD tv = (DWORD)(timeout_s_ * 1000);
    setsockopt((SOCKET)raw_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt((SOCKET)raw_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_s_; tv.tv_usec = 0;
    setsockopt(raw_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(raw_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    bool alive = false;
    if (connect(raw_sock, res->ai_addr, res->ai_addrlen) == 0) {
        std::string req = "GET /health HTTP/1.1\r\nHost: " + host +
                          "\r\nConnection: close\r\n\r\n";
        if (send(raw_sock, req.c_str(), (int)req.size(), 0) > 0) {
            char buf[256] = {};
            int n = recv(raw_sock, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                std::string resp(buf, n);
                alive = (resp.find("HTTP/") != std::string::npos &&
                         resp.find("200")   != std::string::npos);
            }
        }
    }

#ifdef _WIN32
    closesocket((SOCKET)raw_sock);
#else
    close(raw_sock);
#endif
    freeaddrinfo(res);
    return alive;
}
