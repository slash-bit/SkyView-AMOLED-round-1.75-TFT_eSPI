/*
 * SkyView(.ino) firmware
 * Copyright (C) 2019-2021 Linar Yusupov
 *
 * This firmware is essential part of the SoftRF project.
 *
 * Author: Linar Yusupov, linar.r.yusupov@gmail.com
 *
 * Web: http://github.com/lyusupov/SoftRF
 *
 * Credits:
 *   Arduino core for ESP8266 is developed/supported by ESP8266 Community (support-esp8266@esp8266.com)
 *   Arduino Time Library is developed by Paul Stoffregen, http://github.com/PaulStoffregen
 *   TinyGPS++ and PString Libraries are developed by Mikal Hart
 *   Arduino core for ESP32 is developed/supported by Hristo Gochkov
 *   jQuery library is developed by JS Foundation
 *   BCM2835 C library is developed by Mike McCauley
 *   GxEPD2 library is developed by Jean-Marc Zingg
 *   Adafruit GFX library is developed by Adafruit Industries
 *   GDL90 decoder is developed by Ryan David
 *   Sqlite3 Arduino library for ESP32 is developed by Arundale Ramanathan
 *   uCDB library is developed by Ioulianos Kakoulidis
 *   FLN/OGN aircrafts data is courtesy of FlarmNet/GliderNet
 *   Adafruit SSD1306 library is developed by Adafruit Industries
 *   ESP32 I2S WAV player example is developed by Tuan Nha
 *   AceButton library is developed by Brian Park
 *   Flashrom library is part of the flashrom.org project
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
// #include "Arduino.h"
#include "SoCHelper.h"
#include "EEPROMHelper.h"
#include "NMEAHelper.h"
#include "EPDHelper.h"
#include "TrafficHelper.h"
#include "WiFiHelper.h"
#include "WebHelper.h"
#include "BatteryHelper.h"
#include "GDL90Helper.h"
#include "TFTHelper.h"
#include "TouchHelper.h"
#include "BuddyHelper.h"
#include "DemoHelper.h"
#include "SerialDebug.h"

#include "SPIFFS.h"
#include "SkyView.h"
// #include "TFTHelper.h"
#if defined(CPU_float)
extern "C" {
  #include "esp_pm.h"
}
#endif
#if defined(WAVESHARE_AMOLED_1_75)
HWCDC USBSerial;
#endif
hardware_info_t hw_info = {
  .model    = SOFTRF_MODEL_SKYVIEW,
  .revision = HW_REV_UNKNOWN,
  .soc      = SOC_NONE,
  .display  = DISPLAY_NONE
};

bool SPIFFS_is_mounted = false;
bool USB_is_connected = false;  // Defined here, declared as extern in SerialDebug.h

/* Poll input source(s) */
void Input_loop() {
  // Check if demo mode is active
  if (settings->connection == CON_DEMO_FILE) {
    Demo_loop();
    return;
  }

  switch (settings->protocol)
  {
  case PROTOCOL_GDL90:
    GDL90_loop();
    break;
  case PROTOCOL_NMEA:
  default:
    NMEA_loop();
    break;
  }
}


void setup()
{
  // Check if USB is connected before initializing Serial
  USB_is_connected = isUSBConnected();

  if (USB_is_connected) {
    Serial.begin(SERIAL_OUT_BR);
    delay(100);  // Give serial time to initialize
  }

  hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order
  delay(300);

  SerialPrintln();
  SerialPrint(F(SKYVIEW_IDENT));
  SerialPrint(SoC->name);
  SerialPrint(F(" FW.REV: " SKYVIEW_FIRMWARE_VERSION " DEV.ID: "));
  SerialPrintln(String(SoC->getChipId(), HEX));
  SerialPrint(F(" FLASH ID: "));
  SerialPrintln(String(SoC->getFlashId(), HEX));
  SerialPrint(F(" PSRAM FOUND: "));
  SerialPrintln(ESP.getPsramSize() / 1024);
  SerialPrint(F(" FLASH SIZE: "));
  SerialPrint(ESP.getFlashChipSize() / 1024);
  SerialPrintln(F(" KB"));
  if (!SPIFFS.begin(true)) {
    SerialPrintln("SPIFFS Mount Failed");
    SPIFFS_is_mounted = false;
    return;
  }
  SPIFFS_is_mounted = true;
  SerialPrintln("SPIFFS mounted successfully");
  SerialPrint("SPIFFS Total space: ");
  SerialPrintln(SPIFFS.totalBytes());
  SerialPrint("SPIFFS Used space: ");
  SerialPrintln(SPIFFS.usedBytes());

  BuddyManager::readBuddyList("/buddylist.txt");  // Read buddy list from SPIFFS
  // Print buddy list to serial (uses SerialPrint internally)
  BuddyManager::printBuddyList();

  SerialPrintln(F("Copyright (C) 2019-2021 Linar Yusupov. All rights reserved."));

  EEPROM_setup();
  if (settings == NULL || SoC == NULL || SoC->Bluetooth == NULL) {
    SerialPrintln("Error: Null pointer detected!");
    return;
  }

  // Apply demo_mode setting: set connection mode based on demo_mode flag
  if (settings->demo_mode) {
    // Demo mode is ON - set connection to CON_DEMO_FILE
    if (settings->connection != CON_DEMO_FILE) {
      settings->connection = CON_DEMO_FILE;
      EEPROM_store();
      SerialPrintln("Boot: Demo mode ON - connection set to CON_DEMO_FILE");
    }
  } else {
    // Demo mode is OFF - restore to BLE if currently in demo mode
    if (settings->connection == CON_DEMO_FILE) {
      settings->connection = CON_BLUETOOTH_LE;
      EEPROM_store();
      SerialPrintln("Boot: Demo mode OFF - connection set to CON_BLUETOOTH_LE");
    }
  }
  WiFi.mode(WIFI_OFF); //temporarily disable WiFi to prevent issues during Bluetooth init
  btStop();  // Disable classic Bluetooth (we only use BLE)
  Battery_setup();
#if defined(BUTTONS)
  SoC->Button_setup();
#endif /* BUTTONS */
  // Check if demo mode is active
  if (settings->connection == CON_DEMO_FILE) {
    Demo_setup();
  } else {
    switch (settings->protocol)
    {
    case PROTOCOL_GDL90:
      GDL90_setup();
      break;
    case PROTOCOL_NMEA:
    default:
      NMEA_setup();
      break;
    }

    /* If a Dongle is connected - try to wake it up */
    if (settings->connection == CON_SERIAL &&
        settings->protocol   == PROTOCOL_NMEA) {
      SerialInput.write("$PSRFC,?*47\r\n");
      SerialInput.flush();
    }
  }


#if defined(USE_EPAPER)
  SerialPrintln();
  SerialPrint(F("Intializing E-ink display module (may take up to 10 seconds)... "));
  SerialFlush();
  hw_info.display = EPD_setup(true);
#elif defined(USE_TFT)
  TFT_setup();
  hw_info.display = DISPLAY_TFT;
#endif /* USE_EPAPER */
  if (hw_info.display != DISPLAY_NONE) {
    SerialPrintln(F(" done."));
  } else {
    SerialPrintln(F(" failed!"));
  }
#if defined(CPU_float)
  esp_pm_config_esp32s3_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 80,
    .light_sleep_enable = false
  };
  esp_pm_configure(&pm_config);
#endif /* CPU_float */

  WiFi_setup();
#if defined(DB)
  SoC->DB_init();
#endif
#if defined(AUDIO)
  char buf[8];
  strcpy(buf,"POST");
  SoC->TTS(buf);
#endif
  Web_setup();
  Traffic_setup();
#if defined(AMOLED)

  Touch_setup();
#endif

  SoC->WDT_setup();
}

void loop()
{
  if (SoC->Bluetooth) {
    bool wdt_status = loopTaskWDTEnabled;

    if (wdt_status) {
      disableLoopWDT();
    }
    SoC->Bluetooth->loop();

    if (wdt_status) {
      enableLoopWDT();
    }
  }

  Input_loop();

  Traffic_loop();
#if defined(USE_EPAPER)
  EPD_loop();
#elif defined(USE_TFT)
  TFT_loop(); 
#endif /* USE_EPAPER */
  Traffic_ClearExpired();

  WiFi_loop();

  // Handle Web
  Web_loop();
#if defined(BUTTONS)
  SoC->Button_loop();
#endif /* AUDIO */
  Battery_loop();
}

void shutdown(const char *msg)
{
  SoC->WDT_fini();

  /* Shutdown demo mode if active */
  if (settings->connection == CON_DEMO_FILE) {
    Demo_fini();
  }

  /* If a Dongle is connected - try to shut it down */
  if (settings->connection == CON_SERIAL &&
      settings->protocol   == PROTOCOL_NMEA) {
    SerialInput.write("$PSRFC,OFF*37\r\n");
    SerialInput.flush();
  }

  if (SoC->Bluetooth) {
    SoC->Bluetooth->fini();
  }

  Web_fini();
#if defined(DB)
  SoC->DB_fini();
#endif /* DB */
  WiFi_fini();
#if defined(USE_EPAPER)
  EPD_fini(msg);
#elif defined(USE_TFT)
  ESP32_TFT_fini(msg);
#endif /* USE_EPAPER */
#if defined(BUTTONS)
  SoC->Button_fini();
#endif /* BUTTONS */
  battery_fini();
  //TODO: Task delete
  // if (touchTaskHandle != NULL) {
  //   vTaskDelete(touchTaskHandle);
  //   touchTaskHandle = NULL;
  // }
  
  ESP32_fini();
}
