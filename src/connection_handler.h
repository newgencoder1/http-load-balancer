#pragma once
#include "backend_pool.h"
#include <string>

struct ConnectionContext {
    // Use int on Linux, SOCKET (uintptr_t) on Windows —
    // store as uintptr_t to be portable without pulling in winsock here
    uintptr_t   client_fd;
    std::string client_ip;
    BackendPool& pool;
};

void handleConnection(ConnectionContext ctx);
