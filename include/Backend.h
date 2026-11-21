#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum class KeyCode : uint8_t {
  RETURN = 1,
  TAB,
  BACKSPACE,
  ESC,
  RIGHT_ARROW,
  LEFT_ARROW,
  UP_ARROW,
  DOWN_ARROW,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  LEFT_SHIFT,
  LEFT_CTRL,
  LEFT_ALT,
  LEFT_GUI,
};

enum class ConsumerKey : uint8_t {
  PLAY_PAUSE = 1,
  NEXT,
  PREVIOUS,
  VOLUME_UP,
  VOLUME_DOWN,
  MUTE,
};

enum class MouseBtn : uint8_t {
  LEFT = 1,
  RIGHT,
  MIDDLE,
};

// Transport output (BLE GATT Notify + USB CDC Serial)
void backend_send_bytes(const uint8_t* data, size_t len);
void backend_send_text(const char* text);

// Native USB HID actions
void backend_key_write_char(char c);
void backend_key_write(KeyCode key);
void backend_key_hid_stroke(uint8_t modifiers, uint8_t keycode);
void backend_key_press_char(char c);
void backend_key_press(KeyCode key);
void backend_key_release(KeyCode key);
void backend_key_release_all();
void backend_consumer_write(ConsumerKey key);

void backend_mouse_move(int8_t dx, int8_t dy);
void backend_mouse_click(MouseBtn btn);
void backend_mouse_scroll(int8_t amount);

// Timing, hardware, and persistent NVS storage
void backend_delay_ms(uint32_t ms);
uint32_t backend_millis();
void backend_vibrate(uint16_t ms);
void backend_set_ble_name(const char* name);
void backend_set_ble_passkey(uint32_t passkey);
void backend_set_layout(uint8_t layoutId);
uint8_t backend_get_layout();
void backend_macro_save(uint8_t slot, const char* body);
void backend_macro_read(uint8_t slot, char* outBuf, size_t outLen);

// LittleFS script storage (v2.0.0)
bool backend_script_save(const char* name, const char* body, bool append);
bool backend_script_delete(const char* name);
void backend_script_list();
bool backend_script_run(const char* name);

// On-demand Wi-Fi SoftAP Workstation and authenticated OTA (N16R8)
bool backend_wifi_set(bool enabled, const char* ssid, const char* password);
bool backend_ota_set_token(const char* token);
