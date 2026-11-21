#pragma once

#include <stdbool.h>
#include <stdint.h>

void commandExecutor_init();
bool execute_command(const char* payload, uint8_t seq, bool ack = true);

void jiggler_set(bool active);
bool jiggler_get();
void jiggler_poll();
