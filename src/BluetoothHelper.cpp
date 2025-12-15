/*
 * BluetoothHelper.cpp
 * Copyright (C) 2019-2021 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
// #define ESP32
#if defined(ESP32)

#include "Platform_ESP32.h"
#include "SoCHelper.h"
#include "EEPROMHelper.h"
#include "BluetoothHelper.h"
#include "NMEAHelper.h"
#include "GDL90Helper.h"

#include "SkyView.h"
#include <NimBLEDevice.h>

#include "WiFiHelper.h"   // HOSTNAME
#include <NimBLEUtils.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <FS.h>
#include <SPIFFS.h>
#include <set>
#include <DebugLog.h>

std::vector<String> scanResults;

#include <set>

static bool bleInitializedForScan = false;
const NimBLEUUID targetService("0000ffe0-0000-1000-8000-00805f9b34fb");
uint8_t BLEbatteryCache = 0;  // External device battery level in %
unsigned long lastBatteryCheck = 0;
const unsigned long batteryPollInterval = 60000;  // Check every 60 seconds


std::vector<String> scanForBLEDevices(uint32_t scanTimeSeconds) 
{
  // Prevent scanning while a connection is active
  if (ESP32_BT_ctl.status == BT_STATUS_CON) 
  {
    PRINTLN("[BLE] Scan aborted: BLE already connected.");
    return scanResults;
  }

  scanResults.clear();

  // Initialize BLE only once for scanning context
  if (!bleInitializedForScan) 
  {
    NimBLEDevice::init("");  // Empty name
    bleInitializedForScan = true;
  }

  NimBLEScan* scanner = NimBLEDevice::getScan();  
  scanner->setActiveScan(true);
  scanner->setInterval(1349);
  scanner->setWindow(449);
  scanner->clearResults();  // Free memory from last scan


  // disable watchdog during scan (if enabled)
  bool wdt_status = loopTaskWDTEnabled;
  if (wdt_status) 
  {
      disableLoopWDT();
  }
  PRINTLN("[BLE] Starting scan...(10 seconds)");
  NimBLEScanResults results = scanner->getResults(10000, true);  // Convert seconds to milliseconds
  Serial.printf("[BLE] Scan completed: %d device(s) found.\n", results.getCount());
  // enable watchdog again after scan (if it was enabled)
  if (wdt_status) 
  {
    enableLoopWDT();
  }

  std::set<String> uniqueNames;
  for (int i = 0; i < results.getCount(); i++) 
  {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    // Skip devices that don’t advertise the name or service
    if (!device->haveServiceUUID() || !device->isAdvertisingService(targetService)) 
    {
      continue;  // Filter out non-SoftRF devices
    }
    if (device->haveName()) 
    {
      String name = device->getName().c_str();
      name.trim();
      if (name.length() > 0) 
      {
        uniqueNames.insert(name);
      }
    }
  }

  for (const auto& name : uniqueNames) 
  {
    scanResults.push_back(name);
    PRINTLN("[BLE] Found: " + name);
  }
  scanner->clearResults();  // Free memory again
  return scanResults;
}

std::vector<String> allowedBLENames;

void loadAllowedBLENames() 
{
  allowedBLENames.clear();
//  if (!SPIFFS.begin(true)) 
//  {
//    PRINTLN("Failed to mount SPIFFS");
//    return;
//  }
  if (SPIFFS_is_mounted == false) 
  {
    PRINTLN("[BLE] SPIFFS not mounted");
    return;
  }
  File file = SPIFFS.open("/BLEConnections.txt");
  if (!file) 
  {
    PRINTLN("BLEConnections.txt not found");
    return;
  }

  while (file.available()) 
  {
    String name = file.readStringUntil('\n');
    name.trim();
    if (name.length() > 0) 
    {
      allowedBLENames.push_back(name);
      PRINTLN("[BLE] Allowed device: " + name);
    }
  }
  file.close();
}


// BluetoothSerial SerialBT;
String BT_name = HOSTNAME;

Bluetooth_ctl_t ESP32_BT_ctl = 
{
  .mutex   = portMUX_INITIALIZER_UNLOCKED,
  .command = BT_CMD_NONE,
  .status  = BT_STATUS_NC
};

/* LE */
static NimBLERemoteCharacteristic* pRemoteCharacteristic;
static NimBLEAdvertisedDevice* AppDevice;
static NimBLEClient* pClient = nullptr;

static NimBLEUUID serviceUUID(SERVICE_UUID16);
static NimBLEUUID charUUID(CHARACTERISTIC_UUID16);

cbuf *BLE_FIFO_RX, *BLE_FIFO_TX;

static unsigned long BT_TimeMarker = 0;
static unsigned long BLE_Notify_TimeMarker = 0;

#if defined(USE_NIMBLE)
static void AppNotifyCallback(
  NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) 
  {
    if (length > 0) 
    {
      BLE_FIFO_RX->write((char *) pData, (BLE_FIFO_RX->room() > length ?
                                          length : BLE_FIFO_RX->room()));
    }
}

class AppClientCallback : public NimBLEClientCallbacks 
{
  void onConnect(NimBLEClient* pclient) 
  {
  }

  void onDisconnect(NimBLEClient* pclient) 
  {
    ESP32_BT_ctl.status = BT_STATUS_NC;
    PRINTLN(F("BLE: disconnected from Server."));
  }
};

class AppAdvertisedDeviceCallbacks : public NimBLEScanCallbacks 
{
public:

  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override 
  {
    if (!advertisedDevice->haveName()) return;

    String devName = advertisedDevice->getName().c_str();
    bool known = std::any_of(allowedBLENames.begin(), allowedBLENames.end(),
                            [&](const String& name)
                            {
                              return name == devName; 
                            }
    );

    if (known && advertisedDevice->haveServiceUUID() &&
        advertisedDevice->isAdvertisingService(serviceUUID)) 
    {
      PRINTLN(F("  --> Known device and matching service UUID. Connecting..."));
      NimBLEDevice::getScan()->stop();

      delete AppDevice;
      AppDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
      ESP32_BT_ctl.command = BT_CMD_CONNECT;
    }
  }

};
#endif /* USE_NIMBLE */


static bool ESP32_BLEConnectToServer() 
{
  if (!pClient->connect(AppDevice)) 
  {
    PRINTLN(F("BLE: Failed to connect to device."));
    return false;
  }

  NimBLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) 
  {
    Serial.print(F("BLE: Failed to find our service UUID: "));
    PRINTLN(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) 
  {
    Serial.print(F("BLE: Failed to find our characteristic UUID: "));
    PRINTLN(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }

  if(pRemoteCharacteristic->canNotify())
  {
    pRemoteCharacteristic->subscribe(true, AppNotifyCallback);
  }

  ESP32_BT_ctl.status = BT_STATUS_CON;
  return true;
}

static void ESP32_Bluetooth_setup()
{
  switch (settings->connection) 
  {

  case CON_BLUETOOTH_LE:
    {
      loadAllowedBLENames();

      BLE_FIFO_RX = new cbuf(BLE_FIFO_RX_SIZE);
      BLE_FIFO_TX = new cbuf(BLE_FIFO_TX_SIZE);

      esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

#if defined(USE_NIMBLE)
      PRINTLN("[BLE] Initializing NimBLE...");
      NimBLEDevice::init("");
      pClient = NimBLEDevice::createClient();
      pClient->setClientCallbacks(new AppClientCallback());

      NimBLEScan* pBLEScan = NimBLEDevice::getScan();
#else
      BLEDevice::init("");
      pClient = BLEDevice::createClient();
      pClient->setClientCallbacks(new AppClientCallback());

      BLEScan* pBLEScan = BLEDevice::getScan();
#endif /* USE_NIMBLE */

      pBLEScan->setScanCallbacks(new AppAdvertisedDeviceCallbacks(), false);

      pBLEScan->setInterval(1349);
      pBLEScan->setWindow(449);
      pBLEScan->setActiveScan(true);
      pBLEScan->start(1500, false);
      BLE_Notify_TimeMarker = millis();
    }
    break;
  default:
    break;
  }
}

uint8_t getBLEbattery() 
{
  return BLEbatteryCache;
}

void pollBLEbattery() 
{
  if (!pClient || !pClient->isConnected()) 
  {
    PRINTLN("[BLE] Battery poll skipped: not connected");
    return;
  }

  NimBLERemoteService* batteryService = pClient->getService("180F");
  if (!batteryService) 
  {
    PRINTLN("[BLE] Battery service not found");
    return;
  }

  NimBLERemoteCharacteristic* batteryChar = batteryService->getCharacteristic("2A19");
  if (!batteryChar || !batteryChar->canRead()) 
  {
    PRINTLN("[BLE] Battery characteristic not found or unreadable");
    return;
  }

  std::string value = batteryChar->readValue();
  if (!value.empty()) 
  {
    BLEbatteryCache = static_cast<uint8_t>(value[0]);
    Serial.printf("[BLE] Polled battery level: %d%%\n", BLEbatteryCache);
  }
  else 
  {
    PRINTLN("[BLE] Battery level read failed");
  }
}


static void ESP32_Bluetooth_loop()
{

  bool hasData = false;

  switch(settings->connection)
  {
  case CON_BLUETOOTH_LE:
    {
      if (ESP32_BT_ctl.command == BT_CMD_CONNECT) 
      {
        if (ESP32_BLEConnectToServer()) 
        {
          PRINTLN(F("BLE: connected to Server."));
        }
        ESP32_BT_ctl.command = BT_CMD_NONE;
      }

      switch (settings->protocol)
      {
      case PROTOCOL_GDL90:
        hasData = GDL90_isConnected();
        break;
      case PROTOCOL_NMEA:
      default:
        hasData = NMEA_isConnected();
        break;
      }

      if (hasData) {
        BT_TimeMarker = millis();
      }
      else if (millis() - BT_TimeMarker > BT_NODATA_TIMEOUT) 
      {

        PRINTLN(F("BLE: attempt to (re)connect..."));
        if (pClient) 
        {
          if (pClient->isConnected()) 
          {
            pClient->disconnect();
          }
        }

#if defined(USE_NIMBLE)
        NimBLEScan* scan = NimBLEDevice::getScan();
        scan->setScanCallbacks(new AppAdvertisedDeviceCallbacks(), false);
        scan->setActiveScan(true);
        scan->start(50, false);
#else
        BLEDevice::getScan()->start(3, false);
#endif /* USE_NIMBLE */

#if 0
        /* approx. 170 bytes memory leak still remains */
        Serial.print("Free Heap: ");
        PRINTLN(ESP.getFreeHeap());
#endif

        BT_TimeMarker = millis();
      }

      // notify changed value
      // bluetooth stack will go into congestion, if too many packets are sent
      if (ESP32_BT_ctl.status == BT_STATUS_CON &&
          (millis() - BLE_Notify_TimeMarker > 10)) 
          { /* < 18000 baud */

          uint8_t chunk[BLE_MAX_WRITE_CHUNK_SIZE];
          size_t size = (BLE_FIFO_TX->available() < BLE_MAX_WRITE_CHUNK_SIZE ?
                         BLE_FIFO_TX->available() : BLE_MAX_WRITE_CHUNK_SIZE);

          if (size > 0) 
          {
            BLE_FIFO_TX->read((char *) chunk, size);
            pRemoteCharacteristic->writeValue((uint8_t*)chunk, size, false);
            BLE_Notify_TimeMarker = millis();
          }
      }
      if (ESP32_BT_ctl.status == BT_STATUS_CON && millis() - lastBatteryCheck > batteryPollInterval) 
      {
        lastBatteryCheck = millis();
        pollBLEbattery();
      }
    }
    break;
  default:
    break;
  }
}

static void ESP32_Bluetooth_fini()
{
  switch(settings->connection)
  {
  case CON_BLUETOOTH_LE:
    {
      BLEDevice::deinit();
    }
    break;
  default:
    break;
  }
}

static int ESP32_Bluetooth_available()
{
  int rval = 0;

  switch(settings->connection)
  {
  case CON_BLUETOOTH_LE:
    {
      rval = BLE_FIFO_RX->available();
    }
    break;
  default:
    break;
  }

  return rval;
}

static int ESP32_Bluetooth_read()
{
  int rval = -1;

  switch(settings->connection)
  {
  case CON_BLUETOOTH_LE:
    {
      rval = BLE_FIFO_RX->read();
      break;
    }
    break;
  default:
    break;
  }

  return rval;
}

static size_t ESP32_Bluetooth_write(const uint8_t *buffer, size_t size)
{
  int rval = 0;

  switch(settings->connection)
  {
  case CON_BLUETOOTH_LE:
    {
      rval = BLE_FIFO_TX->write((char *) buffer,
                          (BLE_FIFO_TX->room() > size ? size : BLE_FIFO_TX->room()));
    }
    break;
  default:
    break;
  }

  return rval;
}

Bluetooth_ops_t ESP32_Bluetooth_ops = {
  "ESP32 Bluetooth",
  ESP32_Bluetooth_setup,
  ESP32_Bluetooth_loop,
  ESP32_Bluetooth_fini,
  ESP32_Bluetooth_available,
  ESP32_Bluetooth_read,
  ESP32_Bluetooth_write
};

#endif /* ESP32 */
