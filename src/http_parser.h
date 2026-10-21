#pragma once
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool valid = false;
};

struct HttpResponse {
    int status_code = 0;
    std::string status_text;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string raw; // full raw response
    bool valid = false;
};

class HttpParser {
public:
    // Parse raw HTTP request string into HttpRequest struct
    static HttpRequest parseRequest(const std::string& raw);

    // Parse raw HTTP response string into HttpResponse struct
    static HttpResponse parseResponse(const std::string& raw);

    // Build a simple HTTP response string
    static std::string buildResponse(int status, const std::string& body,
                                     const std::string& content_type = "text/plain");

    // Build a forwarded request (adds X-Forwarded-For etc.)
    static std::string buildForwardedRequest(const HttpRequest& req,
                                              const std::string& client_ip,
                                              const std::string& lb_name = "CppLB/1.0");
};
