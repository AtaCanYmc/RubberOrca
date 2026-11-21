#pragma once

#include <stdbool.h>
#include <stdint.h>

enum class KeyboardLayout : uint8_t {
  US = 0,
  TR_Q = 1,
};

constexpr uint8_t HID_MOD_NONE   = 0x00;
constexpr uint8_t HID_MOD_LSHIFT = 0x02;
constexpr uint8_t HID_MOD_RALT   = 0x40;

struct HidStroke {
  uint8_t modifiers;
  uint8_t keycode;
};

// Decode one UTF-8 codepoint and advance *ptr. Returns 0 at end of string.
uint32_t keyLayout_nextCodepoint(const char** ptr);

// Map a Unicode codepoint to a raw USB HID Boot Keyboard usage ID + modifier byte.
bool keyLayout_mapTrQ(uint32_t cp, HidStroke* out);
