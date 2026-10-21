#include "load_balancer.h"
#include "connection_handler.h"
#include <iostream>
#include <thread>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define CLOSE_SOCKET(s) closesocket(s)
  typedef SOCKET SockFd;
  static std::string sockaddrToIP(struct sockaddr_in& a) {
      char buf[INET_ADDRSTRLEN] = {};
      inet_ntop(AF_INET, &a.sin_addr, buf, sizeof(buf));
      return std::string(buf);
  }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define CLOSE_SOCKET(s) close(s)
  #define INVALID_SOCKET (-1)
  typedef int SockFd;
  static std::string sockaddrToIP(struct sockaddr_in& a) {
      return inet_ntoa(a.sin_addr);
  }
#endif

LoadBalancer::LoadBalancer(const LBConfig& config)
    : config_(config),
      health_checker_(pool_, config.health_interval, config.health_timeout)
{
    for (auto& b : config_.backends)
        pool_.addBackend(b.host, b.port);
    pool_.setAlgorithm(config_.algorithm);
}

LoadBalancer::~LoadBalancer() { stop(); }

void LoadBalancer::run() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "[LB] WSAStartup failed\n"; return;
    }
#endif

    SockFd fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) { std::cerr << "[LB] socket() failed\n"; return; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)config_.port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "[LB] bind() failed on port " << config_.port << "\n";
        CLOSE_SOCKET(fd); return;
    }
    if (listen(fd, 128) != 0) {
        std::cerr << "[LB] listen() failed\n";
        CLOSE_SOCKET(fd); return;
    }

    server_fd_ = (uintptr_t)fd;
    running_   = true;
    health_checker_.start();

    std::cout << "========================================\n";
    std::cout << "  C++ HTTP Load Balancer\n";
    std::cout << "  Listening on port " << config_.port << "\n";
    std::cout << "  Algorithm: "
              << (config_.algorithm == LBAlgorithm::ROUND_ROBIN
                      ? "Round Robin" : "Least Connections") << "\n";
    std::cout << "  Backends:\n";
    for (auto& b : config_.backends)
        std::cout << "    -> " << b.host << ":" << b.port << "\n";
    std::cout << "========================================\n";

    acceptLoop();
}

void LoadBalancer::acceptLoop() {
    SockFd fd = (SockFd)server_fd_;
    while (running_) {
        struct sockaddr_in caddr;
        memset(&caddr, 0, sizeof(caddr));
#ifdef _WIN32
        int clen = sizeof(caddr);
#else
        socklen_t clen = sizeof(caddr);
#endif
        SockFd cfd = accept(fd, (struct sockaddr*)&caddr, &clen);
        if (cfd == INVALID_SOCKET) {
            if (running_) std::cerr << "[LB] accept() error\n";
            break;
        }
        std::string client_ip = sockaddrToIP(caddr);
        ConnectionContext ctx{ (uintptr_t)cfd, client_ip, pool_ };
        std::thread t(handleConnection, ctx);
        t.detach();
    }
}

void LoadBalancer::stop() {
    running_ = false;
    health_checker_.stop();
    if (server_fd_ != (uintptr_t)-1) {
        CLOSE_SOCKET((SockFd)server_fd_);
        server_fd_ = (uintptr_t)-1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
