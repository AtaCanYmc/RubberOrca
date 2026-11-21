#pragma once

#include <stdbool.h>

void wifiWorkstation_init(const char* ssid, const char* password, const char* otaToken);
bool wifiWorkstation_setEnabled(bool enabled, const char* ssid, const char* password);
void wifiWorkstation_setToken(const char* token);
bool wifiWorkstation_isActive();
void wifiWorkstation_poll();
