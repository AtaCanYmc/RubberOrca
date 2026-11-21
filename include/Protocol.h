#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t STX = 0x02;
constexpr uint8_t ETX = 0x03;
constexpr uint8_t VERSION = 0x01;

constexpr uint16_t RING_BUF_SIZE = 2048;
constexpr uint16_t PAYLOAD_MAX = 1024;

constexpr uint8_t MACRO_SLOTS = 16;
constexpr uint16_t MACRO_SLOT_SIZE = 1024;

constexpr const char* BLE_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
constexpr const char* BLE_CHAR_UUID    = "0000ffe1-0000-1000-8000-00805f9b34fb";
