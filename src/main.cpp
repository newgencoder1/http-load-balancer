#include "load_balancer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <csignal>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Minimal JSON config parser (no external deps)
// Supports the shape:
// {
//   "port": 8080,
//   "algorithm": "round_robin" | "least_connections",
//   "health_interval": 5,
//   "health_timeout": 2,
//   "backends": [
//     { "host": "backend1", "port": 9001 },
//     ...
//   ]
// }
// ---------------------------------------------------------------------------
static std::string extractValue(const std::string& json,
                                const std::string& key) {
    // find "key": and return the raw value token
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    // skip whitespace and colon
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' ||
                                  json[pos] == '\t' || json[pos] == '\n'))
        ++pos;
    if (pos >= json.size()) return "";
    // read until delimiter
    char end = (json[pos] == '"') ? '"' : 0;
    if (end == '"') { ++pos; }
    std::string val;
    while (pos < json.size()) {
        char c = json[pos++];
        if (end && c == end) break;
        if (!end && (c == ',' || c == '}' || c == '\n')) break;
        val += c;
    }
    // trim
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
        val.pop_back();
    return val;
}

static LBConfig parseConfig(const std::string& path) {
    LBConfig cfg;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Config] Cannot open " << path << " — using defaults\n";
        // Default config: 3 backends on localhost
        cfg.backends = {{"127.0.0.1", 9001},
                        {"127.0.0.1", 9002},
                        {"127.0.0.1", 9003}};
        return cfg;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();

    // port
    std::string port_str = extractValue(json, "port");
    if (!port_str.empty()) cfg.port = std::stoi(port_str);

    // algorithm
    std::string algo = extractValue(json, "algorithm");
    if (algo == "least_connections")
        cfg.algorithm = LBAlgorithm::LEAST_CONNECTIONS;

    // health_interval
    std::string hi = extractValue(json, "health_interval");
    if (!hi.empty()) cfg.health_interval = std::stoi(hi);

    // health_timeout
    std::string ht = extractValue(json, "health_timeout");
    if (!ht.empty()) cfg.health_timeout = std::stoi(ht);

    // backends array — find each { "host": ..., "port": ... } block
    auto arr_start = json.find("\"backends\"");
    if (arr_start != std::string::npos) {
        arr_start = json.find('[', arr_start);
        auto arr_end = json.find(']', arr_start);
        std::string arr = json.substr(arr_start, arr_end - arr_start + 1);

        std::string::size_type pos = 0;
        while ((pos = arr.find('{', pos)) != std::string::npos) {
            auto obj_end = arr.find('}', pos);
            std::string obj = arr.substr(pos, obj_end - pos + 1);

            std::string host = extractValue(obj, "host");
            std::string bp   = extractValue(obj, "port");
            if (!host.empty() && !bp.empty()) {
                cfg.backends.push_back({host, std::stoi(bp)});
            }
            pos = obj_end + 1;
        }
    }

    if (cfg.backends.empty()) {
        std::cerr << "[Config] No backends found, using defaults\n";
        cfg.backends = {{"127.0.0.1", 9001},
                        {"127.0.0.1", 9002},
                        {"127.0.0.1", 9003}};
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// CLI arg parsing
// ---------------------------------------------------------------------------
static void printHelp(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "  --config <path>   Path to config.json (default: config.json)\n"
              << "  --port   <port>   Override listen port\n"
              << "  --algo   rr|lc    Override algorithm (rr=round_robin, lc=least_conn)\n"
              << "  --help            Show this help\n";
}

// Global pointer for signal handler
static LoadBalancer* g_lb = nullptr;

static void sigHandler(int) {
    std::cout << "\n[Main] Shutdown signal received.\n";
    if (g_lb) g_lb->stop();
}

int main(int argc, char* argv[]) {
    std::string config_path = "config.json";
    int    override_port    = 0;
    std::string override_algo;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) { printHelp(argv[0]); return 0; }
        else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            config_path = argv[++i];
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            override_port = std::stoi(argv[++i]);
        else if (strcmp(argv[i], "--algo") == 0 && i + 1 < argc)
            override_algo = argv[++i];
    }

    LBConfig cfg = parseConfig(config_path);
    if (override_port > 0)    cfg.port = override_port;
    if (override_algo == "lc") cfg.algorithm = LBAlgorithm::LEAST_CONNECTIONS;
    if (override_algo == "rr") cfg.algorithm = LBAlgorithm::ROUND_ROBIN;

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    LoadBalancer lb(cfg);
    g_lb = &lb;
    lb.run();   // blocking

    std::cout << "[Main] Load balancer exited cleanly.\n";
    return 0;
}
