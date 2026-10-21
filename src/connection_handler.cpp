#include "connection_handler.h"
#include "http_parser.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define CLOSE_SOCKET(s) closesocket(s)
  typedef SOCKET SockFd;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #define CLOSE_SOCKET(s) close(s)
  #define INVALID_SOCKET  (-1)
  typedef int SockFd;
#endif

static const int READ_TIMEOUT_S  = 10;
static const int MAX_REQUEST_SIZE = 65536;

static std::string readAll(SockFd fd, int timeout_s) {
#ifdef _WIN32
    DWORD tv = (DWORD)(timeout_s * 1000);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv{ timeout_s, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
#endif
    std::string data;
    char buf[4096];
    while (true) {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        data.append(buf, n);
        if ((int)data.size() >= MAX_REQUEST_SIZE) break;
        if (n < (int)sizeof(buf)) break;
    }
    return data;
}

static SockFd connectToBackend(const std::string& host, int port) {
    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(port);

    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
        return INVALID_SOCKET;

    SockFd sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) { freeaddrinfo(res); return INVALID_SOCKET; }

#ifdef _WIN32
    DWORD tv = 5000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv{ 5, 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
#endif

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        CLOSE_SOCKET(sock);
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }
    freeaddrinfo(res);
    return sock;
}

void handleConnection(ConnectionContext ctx) {
    SockFd client_fd = (SockFd)ctx.client_fd;

    std::string raw = readAll(client_fd, READ_TIMEOUT_S);
    if (raw.empty()) { CLOSE_SOCKET(client_fd); return; }

    HttpRequest req = HttpParser::parseRequest(raw);
    if (!req.valid) {
        std::string err = HttpParser::buildResponse(400, "Bad Request\n");
        send(client_fd, err.c_str(), (int)err.size(), 0);
        CLOSE_SOCKET(client_fd);
        return;
    }

    // /stats served locally
    if (req.path == "/stats") {
        auto backends = ctx.pool.getAll();
        std::string body = "=== Load Balancer Stats ===\n";
        body += "Algorithm: ";
        body += (ctx.pool.getAlgorithm() == LBAlgorithm::ROUND_ROBIN)
                    ? "Round Robin\n\n" : "Least Connections\n\n";
        body += "Backends:\n";
        for (int i = 0; i < (int)backends.size(); ++i) {
            body += "  [" + std::to_string(i) + "] " +
                    backends[i].host + ":" + std::to_string(backends[i].port);

            body += "\nstatus=" + std::string(
            backends[i].status == BackendStatus::HEALTHY ? "HEALTHY" : "UNHEALTHY");

            body += "\nactive=" + std::to_string(
                    backends[i].active_connections.load());

            body += "\nlatency(ms)=" + std::to_string(
                    backends[i].latency_ms.load());

            body += "\nfailures=" + std::to_string(
                    backends[i].failure_count.load());

            body += "\n\n";
        }
        
        std::string resp = HttpParser::buildResponse(200, body);
        send(client_fd, resp.c_str(), (int)resp.size(), 0);
        CLOSE_SOCKET(client_fd);
        return;
    }

    int backend_idx = ctx.pool.selectBackend();
    if (backend_idx < 0) {
        std::string err = HttpParser::buildResponse(503, "No healthy backends\n");
        send(client_fd, err.c_str(), (int)err.size(), 0);
        CLOSE_SOCKET(client_fd);
        return;
    }

    auto backends = ctx.pool.getAll();
    std::string bhost = backends[backend_idx].host;
    int         bport = backends[backend_idx].port;
    ctx.pool.incrementConnections(backend_idx);

    SockFd backend_fd = connectToBackend(bhost, bport);
    if (backend_fd == INVALID_SOCKET) {
        std::cerr << "[Handler] Cannot connect to " << bhost << ":" << bport << "\n";
        ctx.pool.markUnhealthy(backend_idx);
        ctx.pool.decrementConnections(backend_idx);
        std::string err = HttpParser::buildResponse(502, "Bad Gateway\n");
        send(client_fd, err.c_str(), (int)err.size(), 0);
        CLOSE_SOCKET(client_fd);
        return;
    }

    std::string forwarded = HttpParser::buildForwardedRequest(req, ctx.client_ip);
    send(backend_fd, forwarded.c_str(), (int)forwarded.size(), 0);

    char buf[4096];
    int  n;
#ifdef _WIN32
    DWORD rtv = READ_TIMEOUT_S * 1000;
    setsockopt(backend_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rtv, sizeof(rtv));
#else
    struct timeval rtv{ READ_TIMEOUT_S, 0 };
    setsockopt(backend_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&rtv, sizeof(rtv));
#endif

    while ((n = recv(backend_fd, buf, sizeof(buf), 0)) > 0) {
        int sent = 0;
        while (sent < n) {
            int w = send(client_fd, buf + sent, n - sent, 0);
            if (w <= 0) goto cleanup;
            sent += w;
        }
    }

cleanup:
    ctx.pool.decrementConnections(backend_idx);
    CLOSE_SOCKET(backend_fd);
    CLOSE_SOCKET(client_fd);
    std::cout << "[Handler] " << ctx.client_ip << " -> "
              << req.method << " " << req.path
              << " -> backend[" << backend_idx << "] "
              << bhost << ":" << bport << "\n";
}
