#if defined(ARDUINO_ARCH_ESP32) && defined(RO_ENABLE_WIFI_WORKSTATION)

#include "WifiWorkstation.h"
#include "CommandExecutor.h"
#include "DuckyRunner.h"
#include "OtterDeckUi.h"
#include "ScriptStore.h"
#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <string.h>

namespace {

constexpr uint8_t MAX_WS_CLIENTS = 4;

WebServer httpServer(80);
WebSocketsServer wsServer(81);

bool apActive = false;
bool otaPendingReboot = false;
uint32_t otaRebootAtMs = 0;

String apSsid = "RubberOrca-AP";
String apPass = "";
String authToken = "";
bool wsAuthed[MAX_WS_CLIENTS] = {};
bool otaUploadAuthorized = false;

bool checkHttpAuth() {
  if (authToken.length() < 12) {
    httpServer.send(403, "text/plain", "Token not configured. Set via: ota token \"<12+ chars>\"");
    return false;
  }
  String provided = httpServer.header("X-Orca-Token");
  if (!constant_time_equals(provided.c_str(), authToken.c_str())) {
    httpServer.send(401, "text/plain", "Unauthorized");
    return false;
  }
  return true;
}

void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (num >= MAX_WS_CLIENTS) return;
  if (type == WStype_DISCONNECTED) {
    wsAuthed[num] = false;
    return;
  }
  if (type == WStype_CONNECTED) {
    wsAuthed[num] = false;
    wsServer.sendTXT(num, "AUTH_REQUIRED");
    return;
  }
  if (type == WStype_TEXT) {
    String msg;
    msg.reserve(length + 1);
    for (size_t i = 0; i < length; ++i) msg += static_cast<char>(payload[i]);
    msg.trim();

    if (!wsAuthed[num]) {
      if (authToken.length() >= 12 && msg.startsWith("auth ") &&
          constant_time_equals(msg.substring(5).c_str(), authToken.c_str())) {
        wsAuthed[num] = true;
        wsServer.sendTXT(num, "AUTH_OK");
      } else {
        wsServer.sendTXT(num, "AUTH_FAIL");
      }
      return;
    }

    bool ok = execute_command(msg.c_str(), 0, false);
    wsServer.sendTXT(num, ok ? "OK" : "ERR");
  }
}

void registerRoutes() {
  static const char* headerKeys[] = {"X-Orca-Token"};
  httpServer.collectHeaders(headerKeys, 1);

  httpServer.on("/", HTTP_GET, []() {
    httpServer.send_P(200, "text/html; charset=utf-8", OTTERDECK_HTML);
  });

  httpServer.on("/api/cmd", HTTP_POST, []() {
    if (!checkHttpAuth()) return;
    String body = httpServer.arg("plain");
    bool ok = execute_command(body.c_str(), 0, false);
    httpServer.send(ok ? 200 : 400, "text/plain", ok ? "OK" : "ERR");
  });

  httpServer.on("/api/scripts", HTTP_GET, []() {
    if (!checkHttpAuth()) return;
    httpServer.send(200, "application/json", scriptStore_listJson());
  });

  httpServer.on("/api/scripts", HTTP_POST, []() {
    if (!checkHttpAuth()) return;
    String name = httpServer.arg("name");
    String body = httpServer.arg("plain");
    bool ok = scriptStore_save(name.c_str(), body.c_str(), false);
    httpServer.send(ok ? 200 : 400, "text/plain", ok ? "SAVED" : "INVALID_SCRIPT");
  });

  httpServer.on("/api/scripts", HTTP_DELETE, []() {
    if (!checkHttpAuth()) return;
    String name = httpServer.arg("name");
    bool ok = scriptStore_delete(name.c_str());
    httpServer.send(ok ? 200 : 404, "text/plain", ok ? "DELETED" : "NOT_FOUND");
  });

  httpServer.on(
      "/api/ota", HTTP_POST,
      []() {
        if (!otaUploadAuthorized) {
          httpServer.send(401, "text/plain", "Unauthorized OTA");
          return;
        }
        bool ok = !Update.hasError();
        httpServer.send(ok ? 200 : 500, "text/plain", ok ? "OTA_OK_REBOOTING" : "OTA_FAILED");
        if (ok) {
          otaPendingReboot = true;
          otaRebootAtMs = millis() + 600;
        }
      },
      []() {
        HTTPUpload& upload = httpServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
          otaUploadAuthorized = (authToken.length() >= 12) &&
                                constant_time_equals(httpServer.header("X-Orca-Token").c_str(),
                                                     authToken.c_str());
          if (!otaUploadAuthorized) return;
          Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        } else if (upload.status == UPLOAD_FILE_WRITE && otaUploadAuthorized) {
          Update.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END && otaUploadAuthorized) {
          Update.end(true);
        } else if (upload.status == UPLOAD_FILE_ABORTED && otaUploadAuthorized) {
          Update.abort();
        }
      });
}

}  // namespace

void wifiWorkstation_init(const char* ssid, const char* password, const char* otaToken) {
  if (ssid && *ssid) apSsid = ssid;
  if (password && *password) apPass = password;
  if (otaToken) authToken = otaToken;
  registerRoutes();
  // SoftAP stays OFF by default on boot for RF stealth and power savings.
  WiFi.mode(WIFI_OFF);
  apActive = false;
}

bool wifiWorkstation_setEnabled(bool enabled, const char* ssid, const char* password) {
  if (ssid && *ssid) apSsid = ssid;
  if (password && *password) apPass = password;

  if (!enabled) {
    if (apActive) {
      wsServer.close();
      httpServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
      apActive = false;
    }
    return true;
  }

  // Enforce WPA2-PSK (min 8 chars) — never open an unencrypted AP.
  if (apPass.length() < 8) return false;

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(apSsid.c_str(), apPass.c_str())) return false;
  httpServer.begin();
  wsServer.begin();
  wsServer.onEvent(handleWsEvent);
  apActive = true;
  return true;
}

void wifiWorkstation_setToken(const char* token) {
  authToken = token ? String(token) : String("");
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; ++i) wsAuthed[i] = false;
}

bool wifiWorkstation_isActive() {
  return apActive;
}

void wifiWorkstation_poll() {
  if (!apActive) return;
  httpServer.handleClient();
  wsServer.loop();
  if (otaPendingReboot && (int32_t)(millis() - otaRebootAtMs) >= 0) {
    ESP.restart();
  }
}

#endif  // ARDUINO_ARCH_ESP32 && RO_ENABLE_WIFI_WORKSTATION
