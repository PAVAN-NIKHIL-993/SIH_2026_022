#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
class WebServer {
 public:
  explicit WebServer(int) {}
  void on(const char *path, HTTPMethod m, std::function<void()> fn) { routes.push_back({path, m, fn}); }
  void onNotFound(std::function<void()> fn) { notFound = fn; }
  void begin() { begun = true; }
  void handleClient() {}
  void send(int code, const char *type, const String &body) { lastCode = code; lastType = type; lastBody = body.s; }
  void send(int code, const char *type, const char *body) { send(code, type, String(body)); }
  void send_P(int code, const char *type, const char *body) { send(code, type, String(body)); }
  void sendHeader(const char *k, const char *v) { headers[k] = v; }
  bool hasArg(const char *n) { return args.count(n) > 0; }
  String arg(const char *n) { return args.count(n) ? String(args[n]) : String(); }

  // ---- test hooks ----
  struct Route { std::string path; HTTPMethod m; std::function<void()> fn; };
  std::vector<Route> routes;
  std::function<void()> notFound;
  std::map<std::string, std::string> args, headers;
  bool begun = false;
  int lastCode = 0; std::string lastType, lastBody;
  int request(HTTPMethod m, const std::string &path, std::map<std::string, std::string> a = {}) {
    args = a; headers.clear(); lastCode = 0; lastBody.clear(); lastType.clear();
    for (auto &r : routes)
      if (r.path == path && (r.m == HTTP_ANY || r.m == m)) { r.fn(); return lastCode; }
    if (notFound) notFound();
    return lastCode;
  }
};
