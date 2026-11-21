#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void bleTransport_init(const char* deviceName, uint32_t passkey = 0);
size_t bleTransport_read(uint8_t* outBuf, size_t maxLen);
void bleTransport_send(const uint8_t* data, size_t len);
void bleTransport_setName(const char* name);
bool bleTransport_isConnected();
