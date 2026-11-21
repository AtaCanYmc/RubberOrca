#pragma once

#include <stdbool.h>
#include <stddef.h>

void scriptStore_init();
bool scriptStore_save(const char* name, const char* body, bool append);
bool scriptStore_delete(const char* name);
void scriptStore_listText();
bool scriptStore_run(const char* name);

#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
String scriptStore_listJson();
bool scriptStore_readFile(const char* name, String& outContent);
#endif
