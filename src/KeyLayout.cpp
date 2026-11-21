#include "KeyLayout.h"

uint32_t keyLayout_nextCodepoint(const char** ptr) {
  if (!ptr || !*ptr || !**ptr) return 0;
  const uint8_t* s = reinterpret_cast<const uint8_t*>(*ptr);
  uint8_t c = s[0];
  if (c < 0x80) {
    *ptr += 1;
    return c;
  }
  if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
    uint32_t cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
    *ptr += 2;
    return cp;
  }
  if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
    uint32_t cp = ((uint32_t)(c & 0x0F) << 12) |
                  ((uint32_t)(s[1] & 0x3F) << 6) |
                  (uint32_t)(s[2] & 0x3F);
    *ptr += 3;
    return cp;
  }
  *ptr += 1;
  return c;
}

bool keyLayout_mapTrQ(uint32_t cp, HidStroke* out) {
  if (!out) return false;

  // Turkish-specific letters (UTF-8 codepoints)
  switch (cp) {
    case 0x011F: *out = {HID_MOD_NONE,   0x2F}; return true;  // ğ
    case 0x011E: *out = {HID_MOD_LSHIFT, 0x2F}; return true;  // Ğ
    case 0x00FC: *out = {HID_MOD_NONE,   0x30}; return true;  // ü
    case 0x00DC: *out = {HID_MOD_LSHIFT, 0x30}; return true;  // Ü
    case 0x015F: *out = {HID_MOD_NONE,   0x33}; return true;  // ş
    case 0x015E: *out = {HID_MOD_LSHIFT, 0x33}; return true;  // Ş
    case 0x0131: *out = {HID_MOD_NONE,   0x0C}; return true;  // ı (dotless i on US 'i' key)
    case 'I':    *out = {HID_MOD_LSHIFT, 0x0C}; return true;  // I
    case 'i':    *out = {HID_MOD_NONE,   0x34}; return true;  // i (dotted i on US '\'' key)
    case 0x0130: *out = {HID_MOD_LSHIFT, 0x34}; return true;  // İ
    case 0x00F6: *out = {HID_MOD_NONE,   0x36}; return true;  // ö
    case 0x00D6: *out = {HID_MOD_LSHIFT, 0x36}; return true;  // Ö
    case 0x00E7: *out = {HID_MOD_NONE,   0x37}; return true;  // ç
    case 0x00C7: *out = {HID_MOD_LSHIFT, 0x37}; return true;  // Ç
    default: break;
  }

  // Standard Latin letters (excluding i/I handled above)
  if (cp >= 'a' && cp <= 'z') {
    *out = {HID_MOD_NONE, static_cast<uint8_t>(0x04 + (cp - 'a'))};
    return true;
  }
  if (cp >= 'A' && cp <= 'Z') {
    *out = {HID_MOD_LSHIFT, static_cast<uint8_t>(0x04 + (cp - 'A'))};
    return true;
  }

  // Digits
  if (cp >= '1' && cp <= '9') {
    *out = {HID_MOD_NONE, static_cast<uint8_t>(0x1E + (cp - '1'))};
    return true;
  }
  if (cp == '0') {
    *out = {HID_MOD_NONE, 0x27};
    return true;
  }

  // Whitespace and Turkish Q punctuation / AltGr symbols
  switch (cp) {
    case '\n': *out = {HID_MOD_NONE,   0x28}; return true;
    case '\t': *out = {HID_MOD_NONE,   0x2B}; return true;
    case ' ':  *out = {HID_MOD_NONE,   0x2C}; return true;
    case '"':  *out = {HID_MOD_NONE,   0x35}; return true;  // Grave key on TR-Q
    case '!':  *out = {HID_MOD_LSHIFT, 0x1E}; return true;
    case '\'': *out = {HID_MOD_LSHIFT, 0x1F}; return true;
    case '^':  *out = {HID_MOD_LSHIFT, 0x20}; return true;
    case '+':  *out = {HID_MOD_LSHIFT, 0x21}; return true;
    case '%':  *out = {HID_MOD_LSHIFT, 0x22}; return true;
    case '&':  *out = {HID_MOD_LSHIFT, 0x23}; return true;
    case '/':  *out = {HID_MOD_LSHIFT, 0x24}; return true;
    case '(':  *out = {HID_MOD_LSHIFT, 0x25}; return true;
    case ')':  *out = {HID_MOD_LSHIFT, 0x26}; return true;
    case '=':  *out = {HID_MOD_LSHIFT, 0x27}; return true;
    case '*':  *out = {HID_MOD_NONE,   0x2D}; return true;
    case '?':  *out = {HID_MOD_LSHIFT, 0x2D}; return true;
    case '-':  *out = {HID_MOD_NONE,   0x2E}; return true;
    case '_':  *out = {HID_MOD_LSHIFT, 0x2E}; return true;
    case ',':  *out = {HID_MOD_NONE,   0x31}; return true;
    case ';':  *out = {HID_MOD_LSHIFT, 0x31}; return true;
    case '.':  *out = {HID_MOD_NONE,   0x38}; return true;
    case ':':  *out = {HID_MOD_LSHIFT, 0x38}; return true;
    case '<':  *out = {HID_MOD_NONE,   0x64}; return true;  // ISO 102nd key
    case '>':  *out = {HID_MOD_LSHIFT, 0x64}; return true;
    case '|':  *out = {HID_MOD_RALT,   0x64}; return true;
    case '@':  *out = {HID_MOD_RALT,   0x14}; return true;  // AltGr + Q
    case '#':  *out = {HID_MOD_RALT,   0x20}; return true;  // AltGr + 3
    case '$':  *out = {HID_MOD_RALT,   0x21}; return true;  // AltGr + 4
    case '{':  *out = {HID_MOD_RALT,   0x24}; return true;  // AltGr + 7
    case '[':  *out = {HID_MOD_RALT,   0x25}; return true;  // AltGr + 8
    case ']':  *out = {HID_MOD_RALT,   0x26}; return true;  // AltGr + 9
    case '}':  *out = {HID_MOD_RALT,   0x27}; return true;  // AltGr + 0
    case '\\': *out = {HID_MOD_RALT,   0x2D}; return true;  // AltGr + *
    case '~':  *out = {HID_MOD_RALT,   0x30}; return true;  // AltGr + ü
    case '`':  *out = {HID_MOD_RALT,   0x31}; return true;  // AltGr + ,
    default: return false;
  }
}
