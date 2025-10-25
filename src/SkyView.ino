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

/* Poll input source(s) */
void Input_loop() {
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
  Serial.begin(SERIAL_OUT_BR); Serial.println();
  hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order
  delay(300);
 

  Serial.println();
  Serial.print(F(SKYVIEW_IDENT));
  Serial.print(SoC->name);
  Serial.print(F(" FW.REV: " SKYVIEW_FIRMWARE_VERSION " DEV.ID: "));
  Serial.println(String(SoC->getChipId(), HEX));
  Serial.print(F(" FLASH ID: "));
  Serial.println(String(SoC->getFlashId(), HEX));
  Serial.print(F(" PSRAM FOUND: "));
  Serial.println(ESP.getPsramSize() / 1024);
  Serial.print(F(" FLASH SIZE: "));
  Serial.print(ESP.getFlashChipSize() / 1024);
  Serial.println(F(" KB"));
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    SPIFFS_is_mounted = false;
    return;
  }
  SPIFFS_is_mounted = true;
  Serial.println("SPIFFS mounted successfully");
  Serial.print("SPIFFS Total space: ");
  Serial.println(SPIFFS.totalBytes());
  Serial.print("SPIFFS Used space: ");
  Serial.println(SPIFFS.usedBytes());

  BuddyManager::readBuddyList("/buddylist.txt");  // Read buddy list from SPIFFS
  // Print buddy list to serial
  BuddyManager::printBuddyList();

  Serial.println(F("Copyright (C) 2019-2021 Linar Yusupov. All rights reserved."));
  // Serial.flush();

  EEPROM_setup();
  if (settings == NULL || SoC == NULL || SoC->Bluetooth == NULL) {
    Serial.println("Error: Null pointer detected!");
    return;
  }
  //temporary settings
 /* settings->adapter       = ADAPTER_TTGO_T5S;
  settings->connection      = CON_BLUETOOTH_LE;
  settings->bridge          = BRIDGE_NONE;
  settings->baudrate        = B38400;
  settings->protocol        = PROTOCOL_NMEA;
  settings->orientation     = DIRECTION_NORTH_UP;
  settings->units           = UNITS_METRIC;
  settings->vmode           = VIEW_MODE_RADAR;
  settings->zoom            = ZOOM_MEDIUM;
  settings->adb             = DB_NONE;
  settings->idpref          = ID_REG;
  settings->voice           = VOICE_OFF;
  settings->compass          = ANTI_GHOSTING_OFF;
  settings->filter          = TRAFFIC_FILTER_500M;
  settings->power_save      = POWER_SAVE_WIFI;
  settings->team            = 0x46BCDC;
*/
  Battery_setup();
#if defined(BUTTONS)
  SoC->Button_setup();
#endif /* BUTTONS */
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


#if defined(USE_EPAPER)
  Serial.println();
  Serial.print(F("Intializing E-ink display module (may take up to 10 seconds)... "));
  Serial.flush();
  hw_info.display = EPD_setup(true);
#elif defined(USE_TFT)
  TFT_setup();
  hw_info.display = DISPLAY_TFT;
#endif /* USE_EPAPER */
  if (hw_info.display != DISPLAY_NONE) {
    Serial.println(F(" done."));
  } else {
    Serial.println(F(" failed!"));
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
