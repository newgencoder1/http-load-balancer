// Lightweight C++ HTTP echo backend server — Windows + Linux compatible
// Usage: ./backend_server <port>
#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <sstream>
#include <atomic>
#include <csignal>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  #define CLOSE_SOCKET(s) closesocket(s)
  typedef SOCKET SockFd;
  static std::string getPeerIP(SockFd fd) {
      struct sockaddr_in a; int l = sizeof(a);
      getpeername(fd,(struct sockaddr*)&a,&l);
      char buf[INET_ADDRSTRLEN]={};
      inet_ntop(AF_INET,&a.sin_addr,buf,sizeof(buf));
      return buf;
  }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define CLOSE_SOCKET(s) close(s)
  #define INVALID_SOCKET (-1)
  typedef int SockFd;
  static std::string getPeerIP(SockFd fd) {
      struct sockaddr_in a; socklen_t l=sizeof(a);
      getpeername(fd,(struct sockaddr*)&a,&l);
      return inet_ntoa(a.sin_addr);
  }
#endif

static std::atomic<bool> g_running{true};
static int g_port = 9001;

static std::string buildResponse(int status, const std::string& body) {
    std::string st = (status == 200) ? "OK" : "Not Found";
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << st << "\r\n";
    oss << "Content-Type: text/plain\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "X-Backend-Port: " << g_port << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

static void handleClient(SockFd cfd) {
    char buf[8192] = {};
    int n = recv(cfd, buf, sizeof(buf)-1, 0);
    if (n <= 0) { CLOSE_SOCKET(cfd); return; }

    std::string raw(buf, n);
    std::string method, path;
    std::istringstream ss(raw);
    ss >> method >> path;

    std::string resp;
    if (path == "/health") {
        resp = buildResponse(200, "OK\n");
    } else {
        std::string client_ip = getPeerIP(cfd);
        std::ostringstream body;
        body << "Hello from Backend on port " << g_port << "\n";
        body << "-----------------------------\n";
        body << "Method : " << method << "\n";
        body << "Path   : " << path   << "\n";
        body << "Client : " << client_ip << "\n";
        auto xf = raw.find("X-Forwarded-For:");
        if (xf != std::string::npos) {
            auto end = raw.find("\r\n", xf);
            body << raw.substr(xf, end - xf) << "\n";
        }
        resp = buildResponse(200, body.str());
    }
    send(cfd, resp.c_str(), (int)resp.size(), 0);
    CLOSE_SOCKET(cfd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <port>\n"; return 1; }
    g_port = std::stoi(argv[1]);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    SockFd srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)g_port);

    if (bind(srv,(struct sockaddr*)&addr,sizeof(addr)) != 0) {
        std::cerr << "Bind failed on port " << g_port << "\n"; return 1;
    }
    listen(srv, 64);
    std::cout << "[Backend:" << g_port << "] Listening...\n";

    while (g_running) {
        struct sockaddr_in caddr; memset(&caddr,0,sizeof(caddr));
#ifdef _WIN32
        int clen = sizeof(caddr);
#else
        socklen_t clen = sizeof(caddr);
#endif
        SockFd cfd = accept(srv,(struct sockaddr*)&caddr,&clen);
        if (cfd == INVALID_SOCKET) break;
        std::thread t(handleClient, cfd);
        t.detach();
    }

    CLOSE_SOCKET(srv);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
