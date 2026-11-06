#pragma once
// NVS mock: a key/value map; with T1_PREFS=<file> it survives a "reboot"
// (the next process run), like the real flash.
#include "Arduino.h"
#include <map>
struct Preferences {
  static std::map<std::string, long> &store() { static std::map<std::string, long> m; return m; }
  static int &writes() { static int w = 0; return w; }
  std::string ns;
  bool begin(const char *name, bool readOnly = false) { ns = name; (void)readOnly; load(); return true; }
  void end() {}
  int32_t getInt(const char *k, int32_t d = 0) { auto it = store().find(ns + "/" + k); return it == store().end() ? d : (int32_t)it->second; }
  size_t putInt(const char *k, int32_t v) { store()[ns + "/" + k] = v; writes()++; save(); return 4; }
  uint16_t getUShort(const char *k, uint16_t d = 0) { auto it = store().find(ns + "/" + k); return it == store().end() ? d : (uint16_t)it->second; }
  size_t putUShort(const char *k, uint16_t v) { store()[ns + "/" + k] = v; writes()++; save(); return 2; }
 private:
  static void load() {
    static bool done = false;
    const char *f = getenv("T1_PREFS");
    if (done || !f) return;
    done = true;
    FILE *fp = fopen(f, "r");
    if (!fp) return;
    char k[128]; long v;
    while (fscanf(fp, "%127s %ld", k, &v) == 2) store()[k] = v;
    fclose(fp);
  }
  static void save() {
    const char *f = getenv("T1_PREFS");
    if (!f) return;
    FILE *fp = fopen(f, "w");
    if (!fp) return;
    for (auto &kv : store()) fprintf(fp, "%s %ld\n", kv.first.c_str(), kv.second);
    fclose(fp);
  }
};
