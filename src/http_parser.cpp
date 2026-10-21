#include "http_parser.h"
#include <sstream>
#include <algorithm>

HttpRequest HttpParser::parseRequest(const std::string& raw) {
    HttpRequest req;
    if (raw.empty()) return req;

    std::istringstream stream(raw);
    std::string line;

    if (!std::getline(stream, line)) return req;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream req_line(line);
    req_line >> req.method >> req.path >> req.version;
    if (req.method.empty() || req.path.empty()) return req;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req.headers[key] = val;
        }
    }

    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) {
        int len = std::stoi(it->second);
        req.body.resize(len);
        stream.read(&req.body[0], len);
    }

    req.valid = true;
    return req;
}

HttpResponse HttpParser::parseResponse(const std::string& raw) {
    HttpResponse resp;
    resp.raw = raw;
    if (raw.empty()) return resp;

    std::istringstream stream(raw);
    std::string line;

    if (!std::getline(stream, line)) return resp;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream status_line(line);
    std::string version;
    status_line >> version >> resp.status_code;
    std::getline(status_line, resp.status_text);
    resp.status_text.erase(0, resp.status_text.find_first_not_of(" \t"));

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            resp.headers[key] = val;
        }
    }

    std::string body_buf;
    while (std::getline(stream, line)) body_buf += line + "\n";
    resp.body = body_buf;
    resp.valid = (resp.status_code > 0);
    return resp;
}

std::string HttpParser::buildResponse(int status, const std::string& body,
                                       const std::string& content_type) {
    std::string status_text;
    switch (status) {
        case 200: status_text = "OK"; break;
        case 400: status_text = "Bad Request"; break;
        case 502: status_text = "Bad Gateway"; break;
        case 503: status_text = "Service Unavailable"; break;
        default:  status_text = "Unknown"; break;
    }
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << status_text << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

std::string HttpParser::buildForwardedRequest(const HttpRequest& req,
                                               const std::string& client_ip,
                                               const std::string& lb_name) {
    std::ostringstream oss;
    oss << req.method << " " << req.path << " " << req.version << "\r\n";

    // C++11-compatible iteration (no structured bindings)
    for (auto it = req.headers.begin(); it != req.headers.end(); ++it) {
        if (it->first == "host") continue;
        oss << it->first << ": " << it->second << "\r\n";
    }

    oss << "X-Forwarded-For: " << client_ip << "\r\n";
    oss << "X-Load-Balancer: " << lb_name   << "\r\n";
    oss << "Connection: close\r\n";

    if (!req.body.empty()) {
        oss << "Content-Length: " << req.body.size() << "\r\n\r\n";
        oss << req.body;
    } else {
        oss << "\r\n";
    }
    return oss.str();
}
