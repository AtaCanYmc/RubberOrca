#ifdef ARDUINO_ARCH_ESP32

#include "Backend.h"
#include "BleTransport.h"
#include "Protocol.h"
#include <Arduino.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <stdio.h>
#include <string.h>

#ifndef BLE_DEFAULT_NAME
#define BLE_DEFAULT_NAME "RubberOrca"
#endif

#ifndef RO_USB_PRODUCT
#define RO_USB_PRODUCT "RubberOrca HID"
#endif

#ifndef RO_USB_MANUFACTURER
#define RO_USB_MANUFACTURER "RubberOrca"
#endif

#if !defined(RO_RGB_PIN) && defined(RGB_BUILTIN)
#define RO_RGB_PIN RGB_BUILTIN
#endif

namespace {

USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
USBHIDConsumerControl ConsumerControl;
Preferences prefs;
uint8_t activeLayout = 0;
int8_t lastLedState = -1;

uint8_t mapKeyCode(KeyCode key) {
  switch (key) {
    case KeyCode::RETURN:      return KEY_RETURN;
    case KeyCode::TAB:         return KEY_TAB;
    case KeyCode::BACKSPACE:   return KEY_BACKSPACE;
    case KeyCode::ESC:         return KEY_ESC;
    case KeyCode::RIGHT_ARROW: return KEY_RIGHT_ARROW;
    case KeyCode::LEFT_ARROW:  return KEY_LEFT_ARROW;
    case KeyCode::F3:          return KEY_F3;
    case KeyCode::F5:          return KEY_F5;
    case KeyCode::LEFT_SHIFT:  return KEY_LEFT_SHIFT;
    case KeyCode::LEFT_CTRL:   return KEY_LEFT_CTRL;
    case KeyCode::LEFT_ALT:    return KEY_LEFT_ALT;
    case KeyCode::LEFT_GUI:    return KEY_LEFT_GUI;
  }
  return 0;
}

uint16_t mapConsumerKey(ConsumerKey key) {
  switch (key) {
    case ConsumerKey::PLAY_PAUSE:  return CONSUMER_CONTROL_PLAY_PAUSE;
    case ConsumerKey::NEXT:        return CONSUMER_CONTROL_SCAN_NEXT;
    case ConsumerKey::PREVIOUS:    return CONSUMER_CONTROL_SCAN_PREVIOUS;
    case ConsumerKey::VOLUME_UP:   return CONSUMER_CONTROL_VOLUME_INCREMENT;
    case ConsumerKey::VOLUME_DOWN: return CONSUMER_CONTROL_VOLUME_DECREMENT;
    case ConsumerKey::MUTE:        return CONSUMER_CONTROL_MUTE;
  }
  return 0;
}

uint8_t mapMouseBtn(MouseBtn btn) {
  switch (btn) {
    case MouseBtn::LEFT:   return MOUSE_LEFT;
    case MouseBtn::RIGHT:  return MOUSE_RIGHT;
    case MouseBtn::MIDDLE: return MOUSE_MIDDLE;
  }
  return MOUSE_LEFT;
}

}  // namespace

void esp32Backend_init() {
  prefs.begin("orca", false);

#ifdef RO_DEFAULT_LAYOUT_TR_Q
  activeLayout = prefs.getUChar("kbd_layout", 1);
#else
  activeLayout = prefs.getUChar("kbd_layout", 0);
#endif

#ifdef VIB_PIN
  pinMode(VIB_PIN, OUTPUT);
  digitalWrite(VIB_PIN, LOW);
#endif

#ifdef RO_USB_VID
  USB.VID(RO_USB_VID);
#endif
#ifdef RO_USB_PID
  USB.PID(RO_USB_PID);
#endif
  USB.productName(RO_USB_PRODUCT);
  USB.manufacturerName(RO_USB_MANUFACTURER);
#ifdef RO_USB_SERIAL
  USB.serialNumber(RO_USB_SERIAL);
#endif

  Keyboard.begin();
  Mouse.begin();
  ConsumerControl.begin();
  USB.begin();

  String bleName = prefs.getString("ble_name", BLE_DEFAULT_NAME);
  uint32_t passkey = prefs.getUInt("ble_passkey", 0);
  bleTransport_init(bleName.c_str(), passkey);
}

void esp32Backend_updateStatusLed(bool bleConnected, bool jigglerActive) {
#if !defined(RO_LED_DISABLE) && defined(RO_RGB_PIN)
  int8_t state = jigglerActive ? 2 : (bleConnected ? 1 : 0);
  if (state == lastLedState) return;
  lastLedState = state;
  if (state == 2)      rgbLedWrite(RO_RGB_PIN, 16, 10, 0);   // Jiggler active: dim amber
  else if (state == 1) rgbLedWrite(RO_RGB_PIN, 0, 16, 0);    // BLE connected: dim green
  else                 rgbLedWrite(RO_RGB_PIN, 0, 0, 16);    // Advertising: dim blue
#else
  (void)bleConnected;
  (void)jigglerActive;
#endif
}

void backend_send_bytes(const uint8_t* data, size_t len) {
  if (!data || len == 0) return;
  bleTransport_send(data, len);
  if (Serial) Serial.write(data, len);
}

void backend_send_text(const char* text) {
  if (!text) return;
  size_t len = strlen(text);
  bleTransport_send(reinterpret_cast<const uint8_t*>(text), len);
  if (Serial) Serial.print(text);
}

void backend_key_write_char(char c) {
  Keyboard.write(static_cast<uint8_t>(c));
}

void backend_key_write(KeyCode key) {
  uint8_t code = mapKeyCode(key);
  if (code) Keyboard.write(code);
}

void backend_key_hid_stroke(uint8_t modifiers, uint8_t keycode) {
  KeyReport rep = {};
  rep.modifiers = modifiers;
  rep.keys[0] = keycode;
  Keyboard.sendReport(&rep);
  delay(8);
  KeyReport empty = {};
  Keyboard.sendReport(&empty);
  delay(4);
}

void backend_key_press_char(char c) {
  Keyboard.press(static_cast<uint8_t>(c));
}

void backend_key_press(KeyCode key) {
  uint8_t code = mapKeyCode(key);
  if (code) Keyboard.press(code);
}

void backend_key_release(KeyCode key) {
  uint8_t code = mapKeyCode(key);
  if (code) Keyboard.release(code);
}

void backend_key_release_all() {
  Keyboard.releaseAll();
}

void backend_consumer_write(ConsumerKey key) {
  uint16_t usage = mapConsumerKey(key);
  if (!usage) return;
  ConsumerControl.press(usage);
  delay(15);
  ConsumerControl.release();
}

void backend_mouse_move(int8_t dx, int8_t dy) {
  Mouse.move(dx, dy, 0);
}

void backend_mouse_click(MouseBtn btn) {
  Mouse.click(mapMouseBtn(btn));
}

void backend_mouse_scroll(int8_t amount) {
  Mouse.move(0, 0, amount);
}

void backend_delay_ms(uint32_t ms) {
  delay(ms);
}

uint32_t backend_millis() {
  return millis();
}

void backend_vibrate(uint16_t ms) {
#ifdef VIB_PIN
  digitalWrite(VIB_PIN, HIGH);
  delay(ms);
  digitalWrite(VIB_PIN, LOW);
#else
  (void)ms;
#endif
}

void backend_set_ble_name(const char* name) {
  if (!name || !*name) return;
  prefs.putString("ble_name", name);
  bleTransport_setName(name);
}

void backend_set_ble_passkey(uint32_t passkey) {
  prefs.putUInt("ble_passkey", passkey);
}

void backend_set_layout(uint8_t layoutId) {
  activeLayout = layoutId;
  prefs.putUChar("kbd_layout", layoutId);
}

uint8_t backend_get_layout() {
  return activeLayout;
}

void backend_macro_save(uint8_t slot, const char* body) {
  if (slot >= MACRO_SLOTS || !body) return;
  char key[8];
  snprintf(key, sizeof(key), "m%u", slot);
  prefs.putString(key, body);
}

void backend_macro_read(uint8_t slot, char* outBuf, size_t outLen) {
  if (!outBuf || outLen == 0) return;
  outBuf[0] = '\0';
  if (slot >= MACRO_SLOTS) return;
  char key[8];
  snprintf(key, sizeof(key), "m%u", slot);
  String val = prefs.getString(key, "");
  strncpy(outBuf, val.c_str(), outLen - 1);
  outBuf[outLen - 1] = '\0';
}

#endif  // ARDUINO_ARCH_ESP32
