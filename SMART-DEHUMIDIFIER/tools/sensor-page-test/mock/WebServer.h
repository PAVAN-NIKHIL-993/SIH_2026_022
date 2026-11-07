#pragma once
#include "Arduino.h"
#include <map>
class WebServer {
 public:
  explicit WebServer(int) {}
  void on(const char *path, std::function<void()> fn) { routes[path] = fn; }
  void begin() {}
  void handleClient() {}
  void send(int code, const char *type, const String &body) { lastCode = code; lastType = type; lastBody = body; }
  void send_P(int code, const char *type, const char *body) { send(code, type, String(body)); }
  bool hasArg(const char *n) { return args.count(n) > 0; }
  String arg(const char *n) { return args.count(n) ? String(args[n]) : String(); }
  // test hooks
  std::map<std::string, std::function<void()>> routes;
  std::map<std::string, std::string> args;
  int lastCode = 0; std::string lastType; String lastBody;
};
