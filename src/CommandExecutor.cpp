#include "CommandExecutor.h"
#include "Backend.h"
#include "DuckyRunner.h"
#include "KeyLayout.h"
#include "Protocol.h"
#include "Utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint32_t JIGGLE_INTERVAL_MS = 5000;
constexpr uint8_t MAX_MACRO_DEPTH = 2;
constexpr int MAX_DELAY_MS = 30000;
constexpr int MAX_PRESS_MS = 5000;
constexpr int MAX_VIBRATE_MS = 5000;
constexpr size_t MAX_BLE_NAME_LEN = 29;

int8_t clamp_i8(int v) {
  if (v < -127) return -127;
  if (v > 127) return 127;
  return static_cast<int8_t>(v);
}

bool jigglerActive = false;
uint32_t lastJiggleMs = 0;

void do_jiggle_pulse() {
  backend_mouse_move(20, 0);
  backend_delay_ms(60);
  backend_mouse_move(0, 20);
  backend_delay_ms(60);
  backend_mouse_move(-20, 0);
  backend_delay_ms(60);
  backend_mouse_move(0, -20);
}

void sendHelp() {
  backend_send_text(
      "RubberOrca (ESP32-S3) - Available commands:\n"
      "  help, ?                      - show this help\n"
      "  ble name \"...\"               - set BLE name (persisted in NVS)\n"
      "  ble passkey <6-digit|off>    - set BLE bonding passkey (persisted in NVS)\n"
      "  layout <us|tr_q>             - set USB HID keyboard layout (persisted in NVS)\n"
      "  type \"...\"                   - type text (escapes: \\n, \\t, \\\", \\\\)\n"
      "  delay N                      - delay N ms\n"
      "  enter, tab, backspace        - simple keys\n"
      "  press <mod> [ms]             - press modifier/key (shift, ctrl, alt, gui, right, left, f5)\n"
      "  hold <mod> / release <mod>   - hold or release modifier\n"
      "  vibrate N                    - vibrate motor for N ms\n"
      "  media <cmd>                  - play_pause/volume_up/volume_down/next/prev/mute\n"
      "  mouse move <dx> <dy>         - relative mouse move\n"
      "  mouse click left|right|middle\n"
      "  mouse scroll <amount>\n"
      "  jiggler on|off|toggle        - toggle mouse jiggler\n"
      "  macro define m0..m15 { ... } - save macro to NVS slot\n"
      "  macro run m0..m15            - run macro slot\n"
      "  script save|append <n> {..}  - save/append LittleFS script\n"
      "  script run|del <name>        - run DuckyScript/orca file or delete\n"
      "  script list                  - list LittleFS scripts\n"
      "  wifi on|off|ap \"s\" \"p\"       - control N16R8 Wi-Fi SoftAP (WPA2)\n"
      "  ota token \"<12+>|off\"        - configure authenticated OTA token\n"
      "  Commands may be chained with && or ;\n");
}

void keyboard_type_text(const char* s) {
  if (!s) return;
  bool trq = (backend_get_layout() == static_cast<uint8_t>(KeyboardLayout::TR_Q));
  while (*s) {
    uint32_t cp = 0;
    if (*s == '\\' && s[1] != '\0') {
      s++;
      if (*s == 'n') cp = '\n';
      else if (*s == 't') cp = '\t';
      else cp = static_cast<unsigned char>(*s);
      s++;
    } else if (trq) {
      cp = keyLayout_nextCodepoint(&s);
    } else {
      cp = static_cast<unsigned char>(*s++);
    }

    if (trq) {
      HidStroke stroke;
      if (keyLayout_mapTrQ(cp, &stroke)) {
        backend_key_hid_stroke(stroke.modifiers, stroke.keycode);
      }
    } else {
      backend_key_write_char(static_cast<char>(cp));
    }
  }
}

bool parse_macro_slot(const char* name, uint8_t* outSlot) {
  if (!name || !outSlot) return false;
  if (name[0] != 'm' || !isdigit((unsigned char)name[1])) return false;
  char* endPtr = nullptr;
  long idx = strtol(name + 1, &endPtr, 10);
  if (endPtr == name + 1 || idx < 0 || idx >= MACRO_SLOTS) return false;
  if (*endPtr != '\0' && !isspace((unsigned char)*endPtr) && *endPtr != '{') return false;
  *outSlot = static_cast<uint8_t>(idx);
  return true;
}

bool modifier_to_keycode(const char* name, KeyCode* out) {
  if (!name || !out) return false;
  if (strcmp(name, "shift") == 0) { *out = KeyCode::LEFT_SHIFT; return true; }
  if (strcmp(name, "ctrl") == 0)  { *out = KeyCode::LEFT_CTRL;  return true; }
  if (strcmp(name, "alt") == 0)   { *out = KeyCode::LEFT_ALT;   return true; }
  if (strcmp(name, "gui") == 0 || strcmp(name, "win") == 0) {
    *out = KeyCode::LEFT_GUI;
    return true;
  }
  return false;
}

bool press_modifier_cmd(const char* args) {
  if (!args || !*args) return false;
  char buf[64];
  strncpy(buf, args, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char* s = trim_inplace(buf);
  if (!*s) return false;

  // Check if the trailing token is an integer duration (ms)
  uint16_t ms = 50;
  char* lastSpace = strrchr(s, ' ');
  if (lastSpace) {
    char* tail = lastSpace + 1;
    bool allDigits = (*tail != '\0');
    for (const char* p = tail; *p; ++p) {
      if (!isdigit((unsigned char)*p)) { allDigits = false; break; }
    }
    if (allDigits) {
      int parsed = parse_int(tail);
      if (parsed <= 0 || parsed > MAX_PRESS_MS) return false;
      ms = static_cast<uint16_t>(parsed);
      *lastSpace = '\0';
      s = trim_inplace(s);
    }
  }

  KeyCode mod;
  if (modifier_to_keycode(s, &mod)) {
    backend_key_press(mod);
    backend_delay_ms(ms);
    backend_key_release(mod);
    return true;
  }
  if (strcmp(s, "right") == 0) { backend_key_write(KeyCode::RIGHT_ARROW); return true; }
  if (strcmp(s, "left") == 0)  { backend_key_write(KeyCode::LEFT_ARROW);  return true; }
  if (strcmp(s, "f5") == 0)    { backend_key_write(KeyCode::F5);          return true; }
  if (strcmp(s, "gui l") == 0 || strcmp(s, "win l") == 0) {
    backend_key_press(KeyCode::LEFT_GUI);
    backend_key_press_char('l');
    backend_delay_ms(ms);
    backend_key_release_all();
    return true;
  }
  if (strcmp(s, "gui d") == 0 || strcmp(s, "win d") == 0) {
    backend_key_press(KeyCode::LEFT_GUI);
    backend_key_press_char('d');
    backend_delay_ms(ms);
    backend_key_release_all();
    return true;
  }
  if (strcmp(s, "ctrl shift esc") == 0) {
    backend_key_press(KeyCode::LEFT_CTRL);
    backend_key_press(KeyCode::LEFT_SHIFT);
    backend_key_press(KeyCode::ESC);
    backend_delay_ms(ms);
    backend_key_release_all();
    return true;
  }
  return false;
}

bool consumer_cmd(const char* cmd) {
  if (!cmd) return false;
  if (strcmp(cmd, "volume_up") == 0)   { backend_consumer_write(ConsumerKey::VOLUME_UP);   return true; }
  if (strcmp(cmd, "volume_down") == 0) { backend_consumer_write(ConsumerKey::VOLUME_DOWN); return true; }
  if (strcmp(cmd, "play_pause") == 0)  { backend_consumer_write(ConsumerKey::PLAY_PAUSE);  return true; }
  if (strcmp(cmd, "next") == 0)        { backend_consumer_write(ConsumerKey::NEXT);        return true; }
  if (strcmp(cmd, "prev") == 0)        { backend_consumer_write(ConsumerKey::PREVIOUS);    return true; }
  if (strcmp(cmd, "mute") == 0)        { backend_consumer_write(ConsumerKey::MUTE);        return true; }
  return false;
}

bool execute_chain(const char* payload, uint8_t depth);

bool execute_single(char* s, uint8_t depth) {
  s = trim_inplace(s);
  if (!s || !*s) return false;

  if (iequals(s, "help") || iequals(s, "?")) {
    sendHelp();
    return true;
  }

  if (strncmp(s, "ble name ", 9) == 0) {
    char* p = s + 9;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return false;
    p++;
    char nameBuf[MAX_BLE_NAME_LEN + 1];
    size_t ni = 0;
    while (*p && *p != '"' && ni < MAX_BLE_NAME_LEN) {
      unsigned char ch = static_cast<unsigned char>(*p++);
      if (ch < 0x20 || ch > 0x7E) return false;
      nameBuf[ni++] = static_cast<char>(ch);
    }
    if (*p != '"' || ni == 0) return false;
    nameBuf[ni] = '\0';
    backend_set_ble_name(nameBuf);
    return true;
  }

  if (strncmp(s, "ble passkey ", 12) == 0) {
    char* arg = trim_inplace(s + 12);
    if (iequals(arg, "off") || strcmp(arg, "0") == 0) {
      backend_set_ble_passkey(0);
      return true;
    }
    if (strlen(arg) != 6) return false;
    for (int i = 0; i < 6; ++i) {
      if (!isdigit((unsigned char)arg[i])) return false;
    }
    long pk = strtol(arg, nullptr, 10);
    if (pk < 100000 || pk > 999999) return false;
    backend_set_ble_passkey(static_cast<uint32_t>(pk));
    return true;
  }

  if (strncmp(s, "layout ", 7) == 0) {
    char* arg = trim_inplace(s + 7);
    if (iequals(arg, "us")) {
      backend_set_layout(static_cast<uint8_t>(KeyboardLayout::US));
      return true;
    }
    if (iequals(arg, "tr_q") || iequals(arg, "tr-q") || iequals(arg, "tr")) {
      backend_set_layout(static_cast<uint8_t>(KeyboardLayout::TR_Q));
      return true;
    }
    return false;
  }

  if (strncmp(s, "type \"", 6) == 0) {
    char* p = s + 6;
    static char outBuf[PAYLOAD_MAX + 1];
    size_t oi = 0;
    while (*p && oi < PAYLOAD_MAX) {
      if (*p == '\\' && p[1] != '\0') {
        if (oi + 2 <= PAYLOAD_MAX) {
          outBuf[oi++] = *p++;
          outBuf[oi++] = *p++;
        } else {
          break;
        }
      } else if (*p == '"') {
        break;
      } else {
        outBuf[oi++] = *p++;
      }
    }
    outBuf[oi] = '\0';
    keyboard_type_text(outBuf);
    return true;
  }

  if (strncmp(s, "delay ", 6) == 0) {
    int d = parse_int(s + 6);
    if (d <= 0 || d > MAX_DELAY_MS) return false;
    backend_delay_ms(static_cast<uint32_t>(d));
    return true;
  }

  if (strcmp(s, "enter") == 0)     { backend_key_write(KeyCode::RETURN);    return true; }
  if (strcmp(s, "tab") == 0)       { backend_key_write(KeyCode::TAB);       return true; }
  if (strcmp(s, "backspace") == 0) { backend_key_write(KeyCode::BACKSPACE); return true; }

  if (strncmp(s, "press ", 6) == 0) {
    return press_modifier_cmd(s + 6);
  }

  if (strncmp(s, "hold ", 5) == 0) {
    KeyCode mod;
    if (!modifier_to_keycode(trim_inplace(s + 5), &mod)) return false;
    backend_key_press(mod);
    return true;
  }

  if (strncmp(s, "release ", 8) == 0) {
    KeyCode mod;
    if (!modifier_to_keycode(trim_inplace(s + 8), &mod)) return false;
    backend_key_release(mod);
    return true;
  }

  if (strncmp(s, "vibrate ", 8) == 0) {
    int ms = parse_int(s + 8);
    if (ms < 0 || ms > MAX_VIBRATE_MS) return false;
    backend_vibrate(static_cast<uint16_t>(ms));
    return true;
  }

  if (strncmp(s, "media ", 6) == 0) {
    return consumer_cmd(trim_inplace(s + 6));
  }

  if (strncmp(s, "mouse move ", 11) == 0) {
    char* p = s + 11;
    while (*p && isspace((unsigned char)*p)) p++;
    int dx = parse_int(p);
    while (*p && !isspace((unsigned char)*p)) p++;
    while (*p && isspace((unsigned char)*p)) p++;
    int dy = parse_int(p);
    backend_mouse_move(clamp_i8(dx), clamp_i8(dy));
    return true;
  }

  if (strncmp(s, "mouse click ", 12) == 0) {
    char* btn = trim_inplace(s + 12);
    if (strcmp(btn, "right") == 0) backend_mouse_click(MouseBtn::RIGHT);
    else if (strcmp(btn, "middle") == 0) backend_mouse_click(MouseBtn::MIDDLE);
    else backend_mouse_click(MouseBtn::LEFT);
    return true;
  }

  if (strncmp(s, "mouse scroll ", 13) == 0) {
    int amount = parse_int(s + 13);
    backend_mouse_scroll(clamp_i8(amount));
    return true;
  }

  if (strncmp(s, "jiggler ", 8) == 0) {
    char* sub = trim_inplace(s + 8);
    if (strcmp(sub, "on") == 0)          { jiggler_set(true);           return true; }
    if (strcmp(sub, "off") == 0)         { jiggler_set(false);          return true; }
    if (strcmp(sub, "toggle") == 0)      { jiggler_set(!jiggler_get()); return true; }
    return false;
  }

  if (strncmp(s, "macro define ", 13) == 0) {
    char* p = s + 13;
    while (*p && isspace((unsigned char)*p)) p++;
    uint8_t slot = 0;
    if (!parse_macro_slot(p, &slot)) return false;
    const char* brace = strchr(p, '{');
    const char* endb = brace ? strrchr(p, '}') : nullptr;
    if (!brace || !endb || endb <= brace) return false;
    size_t len = static_cast<size_t>(endb - brace - 1);
    if (len >= MACRO_SLOT_SIZE) len = MACRO_SLOT_SIZE - 1;
    static char body[MACRO_SLOT_SIZE];
    memcpy(body, brace + 1, len);
    body[len] = '\0';
    backend_macro_save(slot, trim_inplace(body));
    return true;
  }

  if (strncmp(s, "macro run ", 10) == 0) {
    if (depth >= MAX_MACRO_DEPTH) return false;
    char* p = trim_inplace(s + 10);
    uint8_t slot = 0;
    if (!parse_macro_slot(p, &slot)) return false;
    char macroBody[MACRO_SLOT_SIZE];
    macroBody[0] = '\0';
    backend_macro_read(slot, macroBody, sizeof(macroBody));
    if (!macroBody[0]) return false;
    return execute_chain(macroBody, depth + 1);
  }

  // LittleFS Script Management: script save|append <name> { ... }
  if (strncmp(s, "script save ", 12) == 0 || strncmp(s, "script append ", 14) == 0) {
    bool isAppend = (s[7] == 'a');
    char* p = s + (isAppend ? 14 : 12);
    while (*p && isspace((unsigned char)*p)) p++;
    char nameBuf[SCRIPT_NAME_MAX + 8];
    size_t ni = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '{' && ni < sizeof(nameBuf) - 1) {
      nameBuf[ni++] = *p++;
    }
    nameBuf[ni] = '\0';
    if (!script_name_is_valid(nameBuf)) return false;
    const char* brace = strchr(p, '{');
    const char* endb = brace ? strrchr(p, '}') : nullptr;
    if (!brace || !endb || endb <= brace) return false;
    size_t len = static_cast<size_t>(endb - brace - 1);
    if (len >= PAYLOAD_MAX) len = PAYLOAD_MAX - 1;
    static char scriptChunk[PAYLOAD_MAX];
    memcpy(scriptChunk, brace + 1, len);
    scriptChunk[len] = '\0';
    return backend_script_save(nameBuf, trim_inplace(scriptChunk), isAppend);
  }

  if (strcmp(s, "script list") == 0) {
    backend_script_list();
    return true;
  }

  if (strncmp(s, "script run ", 11) == 0) {
    char* name = trim_inplace(s + 11);
    if (!script_name_is_valid(name)) return false;
    return backend_script_run(name);
  }

  if (strncmp(s, "script del ", 11) == 0) {
    char* name = trim_inplace(s + 11);
    if (!script_name_is_valid(name)) return false;
    return backend_script_delete(name);
  }

  // Wi-Fi SoftAP Workstation Control (N16R8)
  if (strcmp(s, "wifi on") == 0)  return backend_wifi_set(true, nullptr, nullptr);
  if (strcmp(s, "wifi off") == 0) return backend_wifi_set(false, nullptr, nullptr);
  if (strncmp(s, "wifi ap ", 8) == 0) {
    char* p = s + 8;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return false;
    p++;
    char ssid[33];
    size_t si = 0;
    while (*p && *p != '"' && si < sizeof(ssid) - 1) {
      unsigned char ch = static_cast<unsigned char>(*p++);
      if (ch < 0x20 || ch > 0x7E) return false;
      ssid[si++] = static_cast<char>(ch);
    }
    if (*p != '"' || si == 0) return false;
    ssid[si] = '\0';
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return false;
    p++;
    char pass[64];
    size_t pi = 0;
    while (*p && *p != '"' && pi < sizeof(pass) - 1) {
      unsigned char ch = static_cast<unsigned char>(*p++);
      if (ch < 0x20 || ch > 0x7E) return false;
      pass[pi++] = static_cast<char>(ch);
    }
    if (*p != '"' || pi < 8) return false;  // WPA2-PSK minimum 8 chars
    pass[pi] = '\0';
    return backend_wifi_set(true, ssid, pass);
  }

  // Authenticated OTA Token Configuration (min 12 printable ASCII chars, or "off")
  if (strncmp(s, "ota token ", 10) == 0) {
    char* p = trim_inplace(s + 10);
    if (iequals(p, "off")) return backend_ota_set_token("");
    if (*p != '"') return false;
    p++;
    char tok[65];
    size_t ti = 0;
    while (*p && *p != '"' && ti < sizeof(tok) - 1) {
      unsigned char ch = static_cast<unsigned char>(*p++);
      if (ch < 0x21 || ch > 0x7E) return false;  // No spaces or control chars
      tok[ti++] = static_cast<char>(ch);
    }
    if (*p != '"' || ti < 12) return false;
    tok[ti] = '\0';
    return backend_ota_set_token(tok);
  }

  return false;
}

bool execute_chain(const char* payload, uint8_t depth) {
  if (!payload || !*payload) return false;

  char buf[PAYLOAD_MAX + 1];
  strncpy(buf, payload, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char* segStart = buf;
  bool inQuotes = false;
  int braceDepth = 0;
  bool anyExecuted = false;
  bool allOk = true;

  for (char* p = buf; ; ++p) {
    char c = *p;
    if (inQuotes) {
      if (c == '\\' && p[1] != '\0') {
        p++;
        continue;
      }
      if (c == '"') inQuotes = false;
    } else {
      if (c == '"') {
        inQuotes = true;
      } else if (c == '{') {
        braceDepth++;
      } else if (c == '}' && braceDepth > 0) {
        braceDepth--;
      } else if (braceDepth == 0) {
        bool isAnd = (c == '&' && p[1] == '&');
        bool isSemi = (c == ';');
        if (c == '\0' || isAnd || isSemi) {
          *p = '\0';
          char* trimmed = trim_inplace(segStart);
          if (*trimmed) {
            anyExecuted = true;
            if (!execute_single(trimmed, depth)) allOk = false;
          }
          if (c == '\0') break;
          if (isAnd) p++;
          segStart = p + 1;
          continue;
        }
      }
    }
    if (c == '\0') break;
  }

  return anyExecuted && allOk;
}

}  // namespace

void commandExecutor_init() {
  jigglerActive = false;
  lastJiggleMs = 0;
}

bool execute_command(const char* payload, uint8_t seq, bool ack) {
  bool ok = execute_chain(payload, 0);
  if (ack) sendAck(seq, ok, ok ? 0 : 1);
  return ok;
}

void jiggler_set(bool active) {
  jigglerActive = active;
  if (active) {
    do_jiggle_pulse();
    lastJiggleMs = backend_millis();
  }
}

bool jiggler_get() {
  return jigglerActive;
}

void jiggler_poll() {
  if (!jigglerActive) return;
  uint32_t now = backend_millis();
  if ((uint32_t)(now - lastJiggleMs) >= JIGGLE_INTERVAL_MS) {
    lastJiggleMs = now;
    do_jiggle_pulse();
  }
}
