#pragma once

#include <stdbool.h>
#include <stdint.h>

char* trim_inplace(char* s);
int parse_int(const char* s);
bool iequals(const char* a, const char* b);
void sendAck(uint8_t seq, bool ok, uint8_t code);
