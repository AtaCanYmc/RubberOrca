#ifdef ARDUINO_ARCH_ESP32

#include "Backend.h"
#include "BleTransport.h"
#include "CommandExecutor.h"
#include "PacketParser.h"
#include <Arduino.h>

void esp32Backend_init();
void esp32Backend_updateStatusLed(bool bleConnected, bool jigglerActive);

void setup() {
  Serial.begin(115200);
  esp32Backend_init();
  packetParser_init();
  commandExecutor_init();
  esp32Backend_updateStatusLed(false, false);
  backend_send_text("RubberOrca ready\n");
}

void loop() {
  uint8_t chunk[64];

  size_t nBle = bleTransport_read(chunk, sizeof(chunk));
  if (nBle > 0) {
    packetParser_feed(chunk, nBle);
  }

  size_t nUsb = 0;
  while (Serial && Serial.available() && nUsb < sizeof(chunk)) {
    int b = Serial.read();
    if (b < 0) break;
    chunk[nUsb++] = static_cast<uint8_t>(b);
  }
  if (nUsb > 0) {
    packetParser_feed(chunk, nUsb);
  }

  packetParser_process();
  jiggler_poll();
  esp32Backend_updateStatusLed(bleTransport_isConnected(), jiggler_get());
  delay(1);
}

#endif  // ARDUINO_ARCH_ESP32
