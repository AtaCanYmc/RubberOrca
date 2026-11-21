#include "Backend.h"
#include "CommandExecutor.h"
#include "PacketParser.h"
#include "Protocol.h"
#include <string.h>
#include <string>
#include <vector>
#include <unity.h>

namespace {

struct MockState {
  std::vector<uint8_t> sentBytes;
  std::string sentText;
  std::string typedChars;
  std::vector<KeyCode> keysWritten;
  std::vector<KeyCode> keysPressed;
  std::vector<KeyCode> keysReleased;
  int releaseAllCount = 0;
  std::vector<ConsumerKey> consumerKeys;
  std::vector<std::pair<int8_t, int8_t>> mouseMoves;
  std::vector<MouseBtn> mouseClicks;
  std::vector<int8_t> mouseScrolls;
  uint32_t totalDelayMs = 0;
  uint32_t nowMs = 0;
  uint16_t lastVibrateMs = 0;
  std::string bleName;
  std::string macros[MACRO_SLOTS];

  void reset() {
    sentBytes.clear();
    sentText.clear();
    typedChars.clear();
    keysWritten.clear();
    keysPressed.clear();
    keysReleased.clear();
    releaseAllCount = 0;
    consumerKeys.clear();
    mouseMoves.clear();
    mouseClicks.clear();
    mouseScrolls.clear();
    totalDelayMs = 0;
    nowMs = 0;
    lastVibrateMs = 0;
    bleName.clear();
    for (auto& m : macros) m.clear();
  }
} mock;

std::vector<uint8_t> buildFrame(uint8_t version, uint8_t seq, const std::string& payload, bool badXor = false) {
  std::vector<uint8_t> f;
  f.push_back(STX);
  f.push_back(version);
  f.push_back(seq);
  f.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
  f.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
  uint8_t x = 0;
  for (char c : payload) {
    uint8_t b = static_cast<uint8_t>(c);
    f.push_back(b);
    x ^= b;
  }
  f.push_back(badXor ? static_cast<uint8_t>(x ^ 0xFF) : x);
  f.push_back(ETX);
  return f;
}

void feedFrame(const std::vector<uint8_t>& frame) {
  packetParser_feed(frame.data(), frame.size());
  packetParser_process();
}

void assertAck(uint8_t seq, uint8_t ok, uint8_t code) {
  TEST_ASSERT_EQUAL_UINT(6, mock.sentBytes.size());
  TEST_ASSERT_EQUAL_UINT8(STX, mock.sentBytes[0]);
  TEST_ASSERT_EQUAL_UINT8(VERSION, mock.sentBytes[1]);
  TEST_ASSERT_EQUAL_UINT8(seq, mock.sentBytes[2]);
  TEST_ASSERT_EQUAL_UINT8(ok, mock.sentBytes[3]);
  TEST_ASSERT_EQUAL_UINT8(code, mock.sentBytes[4]);
  TEST_ASSERT_EQUAL_UINT8(ETX, mock.sentBytes[5]);
}

}  // namespace

// Mock Backend implementation
void backend_send_bytes(const uint8_t* data, size_t len) {
  mock.sentBytes.insert(mock.sentBytes.end(), data, data + len);
}
void backend_send_text(const char* text) { mock.sentText += text; }
void backend_key_write_char(char c) { mock.typedChars.push_back(c); }
void backend_key_write(KeyCode key) { mock.keysWritten.push_back(key); }
void backend_key_press_char(char c) { mock.typedChars.push_back(c); }
void backend_key_press(KeyCode key) { mock.keysPressed.push_back(key); }
void backend_key_release(KeyCode key) { mock.keysReleased.push_back(key); }
void backend_key_release_all() { mock.releaseAllCount++; }
void backend_consumer_write(ConsumerKey key) { mock.consumerKeys.push_back(key); }
void backend_mouse_move(int8_t dx, int8_t dy) { mock.mouseMoves.push_back({dx, dy}); }
void backend_mouse_click(MouseBtn btn) { mock.mouseClicks.push_back(btn); }
void backend_mouse_scroll(int8_t amount) { mock.mouseScrolls.push_back(amount); }
void backend_delay_ms(uint32_t ms) { mock.totalDelayMs += ms; mock.nowMs += ms; }
uint32_t backend_millis() { return mock.nowMs; }
void backend_vibrate(uint16_t ms) { mock.lastVibrateMs = ms; }
void backend_set_ble_name(const char* name) { mock.bleName = name; }
void backend_macro_save(uint8_t slot, const char* body) {
  if (slot < MACRO_SLOTS) mock.macros[slot] = body;
}
void backend_macro_read(uint8_t slot, char* outBuf, size_t outLen) {
  if (slot >= MACRO_SLOTS || outLen == 0) return;
  strncpy(outBuf, mock.macros[slot].c_str(), outLen - 1);
  outBuf[outLen - 1] = '\0';
}

void setUp() {
  mock.reset();
  packetParser_init();
  commandExecutor_init();
}

void tearDown() {}

void test_valid_type_frame_and_escapes() {
  feedFrame(buildFrame(VERSION, 7, "type \"hi\\nthere\\t\\\"orca\\\"\""));
  assertAck(7, 1, 0);
  TEST_ASSERT_EQUAL_STRING("hi\nthere\t\"orca\"", mock.typedChars.c_str());
}

void test_bad_version_nack_code_1() {
  feedFrame(buildFrame(0x99, 3, "enter"));
  assertAck(3, 0, 1);
  TEST_ASSERT_TRUE(mock.keysWritten.empty());
}

void test_oversized_payload_nack_code_2() {
  uint8_t hdr[5] = {STX, VERSION, 4, 0x05, 0x00};  // 1280 > PAYLOAD_MAX (1024)
  packetParser_feed(hdr, sizeof(hdr));
  packetParser_process();
  assertAck(4, 0, 2);
}

void test_bad_checksum_nack_code_3() {
  feedFrame(buildFrame(VERSION, 9, "enter", true));
  assertAck(9, 0, 3);
  TEST_ASSERT_TRUE(mock.keysWritten.empty());
}

void test_single_byte_fallbacks() {
  uint8_t cmds[] = {0x11, 0x21, 0x81, 0x84};
  packetParser_feed(cmds, sizeof(cmds));
  packetParser_process();

  TEST_ASSERT_EQUAL_UINT(1, mock.consumerKeys.size());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsumerKey::PLAY_PAUSE),
                          static_cast<uint8_t>(mock.consumerKeys[0]));
  TEST_ASSERT_EQUAL_UINT(1, mock.keysWritten.size());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(KeyCode::RIGHT_ARROW),
                          static_cast<uint8_t>(mock.keysWritten[0]));
  TEST_ASSERT_EQUAL_UINT(1, mock.mouseClicks.size());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MouseBtn::LEFT),
                          static_cast<uint8_t>(mock.mouseClicks[0]));
  TEST_ASSERT_EQUAL_UINT(1, mock.mouseScrolls.size());
  TEST_ASSERT_EQUAL_INT8(1, mock.mouseScrolls[0]);
}

void test_command_chaining_and_quotes() {
  feedFrame(buildFrame(VERSION, 11, "type \"a;b && c\" && delay 50; enter"));
  assertAck(11, 1, 0);
  TEST_ASSERT_EQUAL_STRING("a;b && c", mock.typedChars.c_str());
  TEST_ASSERT_EQUAL_UINT32(50, mock.totalDelayMs);
  TEST_ASSERT_EQUAL_UINT(1, mock.keysWritten.size());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(KeyCode::RETURN),
                          static_cast<uint8_t>(mock.keysWritten[0]));
}

void test_macro_define_and_run_expanded_slot() {
  feedFrame(buildFrame(VERSION, 12, "macro define m15 { type \"ok\" && enter }"));
  assertAck(12, 1, 0);
  TEST_ASSERT_EQUAL_STRING("type \"ok\" && enter", mock.macros[15].c_str());

  mock.sentBytes.clear();
  feedFrame(buildFrame(VERSION, 13, "macro run m15"));
  assertAck(13, 1, 0);
  TEST_ASSERT_EQUAL_STRING("ok", mock.typedChars.c_str());
  TEST_ASSERT_EQUAL_UINT(1, mock.keysWritten.size());
}

void test_ble_name_and_jiggler_poll() {
  feedFrame(buildFrame(VERSION, 14, "ble name \"OrcaMini\""));
  assertAck(14, 1, 0);
  TEST_ASSERT_EQUAL_STRING("OrcaMini", mock.bleName.c_str());

  mock.sentBytes.clear();
  feedFrame(buildFrame(VERSION, 15, "jiggler on"));
  assertAck(15, 1, 0);
  TEST_ASSERT_TRUE(jiggler_get());
  TEST_ASSERT_EQUAL_UINT(4, mock.mouseMoves.size());

  mock.nowMs += 5000;
  jiggler_poll();
  TEST_ASSERT_EQUAL_UINT(8, mock.mouseMoves.size());
}

void test_press_modifier_and_combo() {
  feedFrame(buildFrame(VERSION, 16, "press gui l 80"));
  assertAck(16, 1, 0);
  TEST_ASSERT_EQUAL_UINT(1, mock.keysPressed.size());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(KeyCode::LEFT_GUI),
                          static_cast<uint8_t>(mock.keysPressed[0]));
  TEST_ASSERT_EQUAL_STRING("l", mock.typedChars.c_str());
  TEST_ASSERT_EQUAL_UINT32(80, mock.totalDelayMs);
  TEST_ASSERT_EQUAL_INT(1, mock.releaseAllCount);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_type_frame_and_escapes);
  RUN_TEST(test_bad_version_nack_code_1);
  RUN_TEST(test_oversized_payload_nack_code_2);
  RUN_TEST(test_bad_checksum_nack_code_3);
  RUN_TEST(test_single_byte_fallbacks);
  RUN_TEST(test_command_chaining_and_quotes);
  RUN_TEST(test_macro_define_and_run_expanded_slot);
  RUN_TEST(test_ble_name_and_jiggler_poll);
  RUN_TEST(test_press_modifier_and_combo);
  return UNITY_END();
}
