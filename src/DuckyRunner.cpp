#include "DuckyRunner.h"
#include "Backend.h"
#include "CommandExecutor.h"
#include "KeyLayout.h"
#include "Utils.h"
#include <ctype.h>
#include <string.h>

namespace {

constexpr int DUCKY_MAX_DELAY_MS = 30000;

void type_layout_aware(const char* s) {
  if (!s) return;
  bool trq = (backend_get_layout() == static_cast<uint8_t>(KeyboardLayout::TR_Q));
  while (*s) {
    uint32_t cp = trq ? keyLayout_nextCodepoint(&s) : static_cast<unsigned char>(*s++);
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

bool map_special_key(const char* tok, KeyCode* out) {
  if (!tok || !out) return false;
  if (iequals(tok, "ENTER") || iequals(tok, "RETURN")) { *out = KeyCode::RETURN; return true; }
  if (iequals(tok, "TAB"))       { *out = KeyCode::TAB; return true; }
  if (iequals(tok, "BACKSPACE")) { *out = KeyCode::BACKSPACE; return true; }
  if (iequals(tok, "ESC") || iequals(tok, "ESCAPE")) { *out = KeyCode::ESC; return true; }
  if (iequals(tok, "RIGHT") || iequals(tok, "RIGHTARROW")) { *out = KeyCode::RIGHT_ARROW; return true; }
  if (iequals(tok, "LEFT")  || iequals(tok, "LEFTARROW"))  { *out = KeyCode::LEFT_ARROW; return true; }
  if (iequals(tok, "UP")    || iequals(tok, "UPARROW"))    { *out = KeyCode::UP_ARROW; return true; }
  if (iequals(tok, "DOWN")  || iequals(tok, "DOWNARROW"))  { *out = KeyCode::DOWN_ARROW; return true; }
  if (iequals(tok, "F1"))  { *out = KeyCode::F1; return true; }
  if (iequals(tok, "F2"))  { *out = KeyCode::F2; return true; }
  if (iequals(tok, "F3"))  { *out = KeyCode::F3; return true; }
  if (iequals(tok, "F4"))  { *out = KeyCode::F4; return true; }
  if (iequals(tok, "F5"))  { *out = KeyCode::F5; return true; }
  if (iequals(tok, "F6"))  { *out = KeyCode::F6; return true; }
  if (iequals(tok, "F7"))  { *out = KeyCode::F7; return true; }
  if (iequals(tok, "F8"))  { *out = KeyCode::F8; return true; }
  if (iequals(tok, "F9"))  { *out = KeyCode::F9; return true; }
  if (iequals(tok, "F10")) { *out = KeyCode::F10; return true; }
  if (iequals(tok, "F11")) { *out = KeyCode::F11; return true; }
  if (iequals(tok, "F12")) { *out = KeyCode::F12; return true; }
  return false;
}

bool map_modifier_token(const char* tok, KeyCode* out) {
  if (!tok || !out) return false;
  if (iequals(tok, "GUI") || iequals(tok, "WINDOWS") || iequals(tok, "COMMAND")) {
    *out = KeyCode::LEFT_GUI;
    return true;
  }
  if (iequals(tok, "CTRL") || iequals(tok, "CONTROL")) { *out = KeyCode::LEFT_CTRL; return true; }
  if (iequals(tok, "ALT")  || iequals(tok, "OPTION"))  { *out = KeyCode::LEFT_ALT; return true; }
  if (iequals(tok, "SHIFT")) { *out = KeyCode::LEFT_SHIFT; return true; }
  return false;
}

bool press_combo_line(char* line) {
  char* tok = strtok(line, " \t");
  int count = 0;
  while (tok) {
    KeyCode kc;
    if (map_modifier_token(tok, &kc) || map_special_key(tok, &kc)) {
      backend_key_press(kc);
      count++;
    } else if (iequals(tok, "SPACE")) {
      backend_key_press_char(' ');
      count++;
    } else if (strlen(tok) == 1 && isprint((unsigned char)tok[0])) {
      backend_key_press_char(static_cast<char>(tolower((unsigned char)tok[0])));
      count++;
    } else {
      backend_key_release_all();
      return false;
    }
    tok = strtok(nullptr, " \t");
  }
  if (count == 0) return false;
  backend_delay_ms(40);
  backend_key_release_all();
  return true;
}

}  // namespace

bool script_name_is_valid(const char* name) {
  if (!name || !*name) return false;
  size_t len = strlen(name);
  const char* dot = strchr(name, '.');
  size_t stemLen = dot ? static_cast<size_t>(dot - name) : len;
  if (stemLen == 0 || stemLen > SCRIPT_NAME_MAX) return false;

  for (size_t i = 0; i < stemLen; ++i) {
    unsigned char c = static_cast<unsigned char>(name[i]);
    if (!isalnum(c) && c != '_' && c != '-') return false;
  }

  if (dot) {
    if (strcmp(dot, ".orca") != 0 && strcmp(dot, ".txt") != 0) return false;
  }
  return true;
}

bool constant_time_equals(const char* a, const char* b) {
  if (!a || !b) return false;
  size_t la = strlen(a);
  size_t lb = strlen(b);
  size_t maxLen = (la > lb) ? la : lb;
  uint8_t diff = static_cast<uint8_t>(la ^ lb);
  for (size_t i = 0; i < maxLen; ++i) {
    uint8_t ca = (i < la) ? static_cast<uint8_t>(a[i]) : 0;
    uint8_t cb = (i < lb) ? static_cast<uint8_t>(b[i]) : 0;
    diff |= static_cast<uint8_t>(ca ^ cb);
  }
  return diff == 0;
}

bool ducky_execute_line(const char* rawLine, uint32_t* defaultDelayMs) {
  if (!rawLine) return true;

  char buf[256];
  strncpy(buf, rawLine, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char* s = trim_inplace(buf);
  if (!*s || s[0] == '#') return true;

  // Comments
  if (iequals(s, "REM") || (strncasecmp(s, "REM ", 4) == 0)) return true;

  // DEFAULT_DELAY / DEFAULTDELAY
  if (strncasecmp(s, "DEFAULT_DELAY ", 14) == 0 || strncasecmp(s, "DEFAULTDELAY ", 13) == 0) {
    const char* arg = strchr(s, ' ') + 1;
    int d = parse_int(arg);
    if (d < 0 || d > DUCKY_MAX_DELAY_MS) return false;
    if (defaultDelayMs) *defaultDelayMs = static_cast<uint32_t>(d);
    return true;
  }

  // DELAY <ms>
  if (strncasecmp(s, "DELAY ", 6) == 0) {
    int d = parse_int(s + 6);
    if (d <= 0 || d > DUCKY_MAX_DELAY_MS) return false;
    backend_delay_ms(static_cast<uint32_t>(d));
    return true;
  }

  // STRING / STRINGLN
  bool executed = false;
  if (strncasecmp(s, "STRINGLN ", 9) == 0) {
    type_layout_aware(s + 9);
    backend_key_write(KeyCode::RETURN);
    executed = true;
  } else if (strncasecmp(s, "STRING ", 7) == 0) {
    type_layout_aware(s + 7);
    executed = true;
  } else {
    KeyCode singleKey;
    if (map_special_key(s, &singleKey)) {
      backend_key_write(singleKey);
      executed = true;
    } else if (iequals(s, "SPACE")) {
      backend_key_write_char(' ');
      executed = true;
    } else {
      // Check if first token is a DuckyScript modifier (GUI, CTRL, ALT, SHIFT)
      char copy[256];
      strncpy(copy, s, sizeof(copy) - 1);
      copy[sizeof(copy) - 1] = '\0';
      char* firstSpace = strchr(copy, ' ');
      if (firstSpace) *firstSpace = '\0';
      KeyCode modCheck;
      if (map_modifier_token(copy, &modCheck)) {
        char comboBuf[256];
        strncpy(comboBuf, s, sizeof(comboBuf) - 1);
        comboBuf[sizeof(comboBuf) - 1] = '\0';
        executed = press_combo_line(comboBuf);
      } else {
        // Fallback to native RubberOrca command syntax (e.g. type, media, mouse, jiggler, layout)
        executed = execute_command(s, 0, false);
      }
    }
  }

  if (executed && defaultDelayMs && *defaultDelayMs > 0) {
    backend_delay_ms(*defaultDelayMs);
  }
  return executed;
}
