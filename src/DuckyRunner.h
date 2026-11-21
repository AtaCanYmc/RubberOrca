#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

constexpr size_t SCRIPT_NAME_MAX = 32;
constexpr size_t SCRIPT_FILE_MAX = 32768;

// Validates ^[a-zA-Z0-9_-]{1,32}(\.orca|\.txt)?$ to block path traversal.
bool script_name_is_valid(const char* name);

// Executes a single line of DuckyScript (or native RubberOrca command fallback).
bool ducky_execute_line(const char* rawLine, uint32_t* defaultDelayMs);

// Constant-time string equality for API / OTA token verification.
bool constant_time_equals(const char* a, const char* b);
