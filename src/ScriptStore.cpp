#if defined(ARDUINO_ARCH_ESP32) && defined(RO_ENABLE_LITTLEFS)

#include "ScriptStore.h"
#include "Backend.h"
#include "DuckyRunner.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr const char* SCRIPTS_DIR = "/scripts";
constexpr size_t MAX_SCRIPTS = 64;
bool fsReady = false;

bool makeScriptPath(const char* name, char* out, size_t outLen) {
  if (!script_name_is_valid(name) || !out || outLen < 16) return false;
  int n = snprintf(out, outLen, "%s/%s", SCRIPTS_DIR, name);
  return n > 0 && static_cast<size_t>(n) < outLen;
}

size_t countScripts() {
  File dir = LittleFS.open(SCRIPTS_DIR);
  if (!dir || !dir.isDirectory()) return 0;
  size_t count = 0;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) count++;
    f = dir.openNextFile();
  }
  return count;
}

}  // namespace

void scriptStore_init() {
  fsReady = LittleFS.begin(true);
  if (fsReady && !LittleFS.exists(SCRIPTS_DIR)) {
    LittleFS.mkdir(SCRIPTS_DIR);
  }
}

bool scriptStore_save(const char* name, const char* body, bool append) {
  if (!fsReady || !body) return false;
  char path[64];
  if (!makeScriptPath(name, path, sizeof(path))) return false;

  bool exists = LittleFS.exists(path);
  if (!exists && countScripts() >= MAX_SCRIPTS) return false;

  size_t addLen = strlen(body);
  if (append && exists) {
    File existing = LittleFS.open(path, FILE_READ);
    if (!existing) return false;
    size_t curSize = existing.size();
    existing.close();
    if (curSize + addLen + 1 > SCRIPT_FILE_MAX) return false;
  } else if (addLen > SCRIPT_FILE_MAX) {
    return false;
  }

  File f = LittleFS.open(path, (append && exists) ? FILE_APPEND : FILE_WRITE);
  if (!f) return false;
  if (append && exists) f.write('\n');
  size_t written = f.write(reinterpret_cast<const uint8_t*>(body), addLen);
  f.close();
  return written == addLen;
}

bool scriptStore_delete(const char* name) {
  if (!fsReady) return false;
  char path[64];
  if (!makeScriptPath(name, path, sizeof(path))) return false;
  return LittleFS.exists(path) && LittleFS.remove(path);
}

void scriptStore_listText() {
  if (!fsReady) {
    backend_send_text("LittleFS not mounted\n");
    return;
  }
  File dir = LittleFS.open(SCRIPTS_DIR);
  if (!dir || !dir.isDirectory()) {
    backend_send_text("(no scripts)\n");
    return;
  }
  bool any = false;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      any = true;
      char line[80];
      snprintf(line, sizeof(line), "%s (%u B)\n", f.name(), static_cast<unsigned>(f.size()));
      backend_send_text(line);
    }
    f = dir.openNextFile();
  }
  if (!any) backend_send_text("(no scripts)\n");
}

String scriptStore_listJson() {
  String json = "[";
  if (!fsReady) return "[]";
  File dir = LittleFS.open(SCRIPTS_DIR);
  if (!dir || !dir.isDirectory()) return "[]";
  bool first = true;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      if (!first) json += ",";
      first = false;
      json += "{\"name\":\"";
      json += f.name();
      json += "\",\"size\":";
      json += String(f.size());
      json += "}";
    }
    f = dir.openNextFile();
  }
  json += "]";
  return json;
}

bool scriptStore_readFile(const char* name, String& outContent) {
  if (!fsReady) return false;
  char path[64];
  if (!makeScriptPath(name, path, sizeof(path))) return false;
  File f = LittleFS.open(path, FILE_READ);
  if (!f || f.size() > SCRIPT_FILE_MAX) {
    if (f) f.close();
    return false;
  }
  outContent = f.readString();
  f.close();
  return true;
}

bool scriptStore_run(const char* name) {
  if (!fsReady) return false;
  char path[64];
  if (!makeScriptPath(name, path, sizeof(path))) return false;
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return false;

  uint32_t defaultDelayMs = 0;
  char lineBuf[256];
  bool allOk = true;
  while (f.available()) {
    size_t len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
    lineBuf[len] = '\0';
    if (!ducky_execute_line(lineBuf, &defaultDelayMs)) {
      allOk = false;
      break;
    }
  }
  f.close();
  return allOk;
}

#endif  // ARDUINO_ARCH_ESP32 && RO_ENABLE_LITTLEFS
