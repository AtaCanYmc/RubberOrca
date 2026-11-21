#include "PacketParser.h"
#include "Backend.h"
#include "CommandExecutor.h"
#include "Protocol.h"
#include "Utils.h"

namespace {

uint8_t ringBuf[RING_BUF_SIZE];
uint16_t ringHead = 0;
uint16_t ringTail = 0;

inline bool ringAvailable() { return ringHead != ringTail; }

inline uint16_t ringFree() {
  if (ringHead >= ringTail) return RING_BUF_SIZE - (ringHead - ringTail) - 1;
  return ringTail - ringHead - 1;
}

inline void ringWrite(uint8_t b) {
  ringBuf[ringHead] = b;
  ringHead = (ringHead + 1) % RING_BUF_SIZE;
}

inline uint8_t ringRead() {
  uint8_t v = ringBuf[ringTail];
  ringTail = (ringTail + 1) % RING_BUF_SIZE;
  return v;
}

enum ParserState { WAIT_STX, READ_HDR, READ_PAYLOAD, READ_CHECK, WAIT_ETX };
ParserState state = WAIT_STX;
uint8_t headerBuf[4];
uint8_t hdrIdx = 0;
uint16_t payloadLen = 0;
uint16_t payloadIdx = 0;
uint8_t seqNum = 0;
uint8_t payloadBuf[PAYLOAD_MAX + 1];

void handleSingleByteCommand(uint8_t cmd) {
  switch (cmd) {
    case 0x11: backend_consumer_write(ConsumerKey::PLAY_PAUSE); break;
    case 0x12: backend_consumer_write(ConsumerKey::NEXT); break;
    case 0x13: backend_consumer_write(ConsumerKey::PREVIOUS); break;
    case 0x14: backend_consumer_write(ConsumerKey::VOLUME_UP); break;
    case 0x15: backend_consumer_write(ConsumerKey::VOLUME_DOWN); break;
    case 0x16: backend_consumer_write(ConsumerKey::MUTE); break;
    case 0x21: backend_key_write(KeyCode::RIGHT_ARROW); break;
    case 0x22: backend_key_write(KeyCode::LEFT_ARROW); break;
    case 0x23: backend_key_write(KeyCode::F5); break;
    case 0x24: backend_key_write_char('b'); break;
    case 0x31:
      backend_key_press(KeyCode::LEFT_GUI);
      backend_key_press_char('l');
      backend_delay_ms(40);
      backend_key_release_all();
      backend_key_press(KeyCode::LEFT_CTRL);
      backend_key_press(KeyCode::LEFT_GUI);
      backend_key_press_char('q');
      backend_delay_ms(40);
      backend_key_release_all();
      break;
    case 0x32:
      execute_command("jiggler toggle", 0, false);
      break;
    case 0x33:
      backend_key_press(KeyCode::LEFT_CTRL);
      backend_key_press(KeyCode::LEFT_SHIFT);
      backend_key_press(KeyCode::ESC);
      backend_delay_ms(40);
      backend_key_release_all();
      backend_key_press(KeyCode::LEFT_GUI);
      backend_key_press(KeyCode::LEFT_ALT);
      backend_key_press(KeyCode::ESC);
      backend_delay_ms(40);
      backend_key_release_all();
      break;
    case 0x34:
      backend_key_press(KeyCode::LEFT_GUI);
      backend_key_press_char('d');
      backend_delay_ms(40);
      backend_key_release_all();
      backend_key_press(KeyCode::LEFT_GUI);
      backend_key_press(KeyCode::F3);
      backend_delay_ms(40);
      backend_key_release_all();
      break;
    case 0x35:
      backend_vibrate(100);
      break;
    case 0x41:
      execute_command("type \"b\" && delay 120 && type \"4\" && delay 120 && type \"2\"", 0, false);
      break;
    case 0x81: backend_mouse_click(MouseBtn::LEFT); break;
    case 0x82: backend_mouse_click(MouseBtn::RIGHT); break;
    case 0x83: backend_mouse_click(MouseBtn::MIDDLE); break;
    case 0x84: backend_mouse_scroll(1); break;
    case 0x85: backend_mouse_scroll(-1); break;
    default: break;
  }
}

}  // namespace

void packetParser_init() {
  state = WAIT_STX;
  ringHead = ringTail = 0;
  hdrIdx = 0;
  payloadLen = 0;
  payloadIdx = 0;
  seqNum = 0;
}

size_t packetParser_feed(const uint8_t* data, size_t len) {
  if (!data || len == 0) return 0;
  size_t written = 0;
  while (written < len && ringFree() > 0) {
    ringWrite(data[written++]);
  }
  return written;
}

void packetParser_process() {
  while (ringAvailable()) {
    uint8_t b = ringRead();
    switch (state) {
      case WAIT_STX:
        if (b == STX) {
          hdrIdx = 0;
          payloadLen = 0;
          payloadIdx = 0;
          state = READ_HDR;
        } else if (b >= 0x10 && b <= 0x8F) {
          handleSingleByteCommand(b);
        }
        break;

      case READ_HDR:
        headerBuf[hdrIdx++] = b;
        if (hdrIdx == 4) {
          seqNum = headerBuf[1];
          if (headerBuf[0] != VERSION) {
            sendAck(seqNum, false, 1);
            state = WAIT_STX;
          } else {
            payloadLen = (static_cast<uint16_t>(headerBuf[2]) << 8) |
                         static_cast<uint16_t>(headerBuf[3]);
            if (payloadLen > PAYLOAD_MAX) {
              sendAck(seqNum, false, 2);
              state = WAIT_STX;
            } else {
              payloadIdx = 0;
              state = (payloadLen == 0) ? READ_CHECK : READ_PAYLOAD;
            }
          }
        }
        break;

      case READ_PAYLOAD:
        if (payloadIdx < PAYLOAD_MAX) payloadBuf[payloadIdx++] = b;
        if (payloadIdx == payloadLen) {
          state = READ_CHECK;
        }
        break;

      case READ_CHECK: {
        uint8_t computed = 0;
        for (uint16_t i = 0; i < payloadLen; ++i) computed ^= payloadBuf[i];
        if (computed != b) {
          sendAck(seqNum, false, 3);
          state = WAIT_STX;
        } else {
          payloadBuf[payloadLen] = '\0';
          execute_command(reinterpret_cast<const char*>(payloadBuf), seqNum, true);
          state = WAIT_ETX;
        }
        break;
      }

      case WAIT_ETX:
        if (b == ETX) {
          state = WAIT_STX;
        } else if (b == STX) {
          hdrIdx = 0;
          payloadLen = 0;
          payloadIdx = 0;
          state = READ_HDR;
        } else {
          state = WAIT_STX;
        }
        break;
    }
  }
}
