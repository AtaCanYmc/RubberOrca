#ifdef ARDUINO_ARCH_ESP32

#include "BleTransport.h"
#include "Protocol.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace {

constexpr size_t BLE_RX_BUF_SIZE = 2048;
constexpr size_t BLE_NOTIFY_CHUNK = 20;

uint8_t rxBuf[BLE_RX_BUF_SIZE];
volatile size_t rxHead = 0;
volatile size_t rxTail = 0;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

BLEServer* bleServer = nullptr;
BLECharacteristic* bleChar = nullptr;
volatile bool bleConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* /*server*/) override {
    bleConnected = true;
  }

  void onDisconnect(BLEServer* server) override {
    bleConnected = false;
    server->getAdvertising()->start();
  }
};

class CharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String value = characteristic->getValue();
    size_t len = value.length();
    if (len == 0) return;

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(value.c_str());
    portENTER_CRITICAL(&rxMux);
    for (size_t i = 0; i < len; ++i) {
      size_t next = (rxHead + 1) % BLE_RX_BUF_SIZE;
      if (next == rxTail) break;  // Drop overflow bytes rather than corrupting head
      rxBuf[rxHead] = bytes[i];
      rxHead = next;
    }
    portEXIT_CRITICAL(&rxMux);
  }
};

}  // namespace

void bleTransport_init(const char* deviceName) {
  const char* name = (deviceName && *deviceName) ? deviceName : "RubberOrca";
  BLEDevice::init(name);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* service = bleServer->createService(BLE_SERVICE_UUID);
  bleChar = service->createCharacteristic(
      BLE_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR |
      BLECharacteristic::PROPERTY_NOTIFY);
  bleChar->setCallbacks(new CharCallbacks());
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}

size_t bleTransport_read(uint8_t* outBuf, size_t maxLen) {
  if (!outBuf || maxLen == 0) return 0;
  size_t count = 0;
  portENTER_CRITICAL(&rxMux);
  while (count < maxLen && rxTail != rxHead) {
    outBuf[count++] = rxBuf[rxTail];
    rxTail = (rxTail + 1) % BLE_RX_BUF_SIZE;
  }
  portEXIT_CRITICAL(&rxMux);
  return count;
}

void bleTransport_send(const uint8_t* data, size_t len) {
  if (!bleConnected || !bleChar || !data || len == 0) return;
  size_t offset = 0;
  while (offset < len) {
    size_t chunk = (len - offset > BLE_NOTIFY_CHUNK) ? BLE_NOTIFY_CHUNK : (len - offset);
    bleChar->setValue(const_cast<uint8_t*>(data + offset), chunk);
    bleChar->notify();
    offset += chunk;
    if (offset < len) delay(5);
  }
}

void bleTransport_setName(const char* name) {
  if (!name || !*name) return;
  BLEDevice::stopAdvertising();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->setName(name);
  if (!bleConnected) {
    BLEDevice::startAdvertising();
  }
}

bool bleTransport_isConnected() {
  return bleConnected;
}

#endif  // ARDUINO_ARCH_ESP32
