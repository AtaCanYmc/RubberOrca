#pragma once

#include <stddef.h>
#include <stdint.h>

void packetParser_init();
size_t packetParser_feed(const uint8_t* data, size_t len);
void packetParser_process();
