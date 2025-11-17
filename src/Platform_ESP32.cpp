/*
 * Platform_ESP32.cpp
 * Copyright (C) 2019-2022 Linar Yusupov
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

#include <SPI.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>
#include <rom/spi_flash.h>
#include <soc/adc_channel.h>
#include <flashchips.h>

#include "SoCHelper.h"
// #include "EPDHelper.h"
#include "TFTHelper.h"
#include "EEPROMHelper.h"
#include "WiFiHelper.h"
#include "BluetoothHelper.h"
#include "TouchHelper.h"
#include "BuddyHelper.h"

#include "SkyView.h"
#include <FS.h>
#include <SD.h>
#include <battery.h>
#include "BatteryHelper.h"
// #include <SD.h>

// #include "uCDB.hpp"

#include "driver/i2s.h"

#include <esp_wifi.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <Wire.h>
#include "Arduino_DriveBus_Library.h"
#include "../pins_config.h"

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  28        /* Time ESP32 will go to sleep (in seconds) */

// Forward declarations
static void ESP32_Button_setup();

WebServer server ( 80 );

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

extern TFT_eSPI tft;
extern TFT_eSprite sprite;
static bool wireStarted = false;

bool setupWireIfNeeded(int sda, int scl, int freq)
{
  if (!wireStarted) 
  {
    if (Wire.begin(sda,scl,freq))
    {
      wireStarted = true;
    }
  }
  return wireStarted;
}
#if defined(USE_EPAPER)
/*
 * TTGO-T5S. Pin definition

#define BUSY_PIN        4
#define CS_PIN          5
#define RST_PIN         16
#define DC_PIN          17
#define SCK_PIN         18
#define MOSI_PIN        23

P1-1                    21
P1-2                    22 (LED)

I2S MAX98357A           26
                        25
                        19

I2S MIC                 27
                        32
                        33

B0                      RST
B1                      38
B2                      37
B3                      39

SD                      2
                        13
                        14
                        15

P2                      0
                        12
                        13
                        RXD
                        TXD
                        34
                        35 (BAT)
 */
GxEPD2_BW<GxEPD2_270, GxEPD2_270::HEIGHT> epd_ttgo_t5s_W3(GxEPD2_270(/*CS=5*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));
GxEPD2_BW<GxEPD2_270_T91, GxEPD2_270_T91::HEIGHT> epd_ttgo_t5s_T91(GxEPD2_270_T91(/*CS=5*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

/*
 * Waveshare E-Paper ESP32 Driver Board

#define SCK_PIN         13
#define MOSI_PIN        14
#define CS_PIN          15
#define BUSY_PIN        25
#define RST_PIN         26
#define DC_PIN          27

B1                      0
LED                     2

RX0, TX0                3,1

P                       0,2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33,34,35
 */
GxEPD2_BW<GxEPD2_270, GxEPD2_270::HEIGHT> epd_waveshare_W3(GxEPD2_270(/*CS=15*/ 15, /*DC=*/ 27, /*RST=*/ 26, /*BUSY=*/ 25));
GxEPD2_BW<GxEPD2_270_T91, GxEPD2_270_T91::HEIGHT> epd_waveshare_T91(GxEPD2_270_T91(/*CS=15*/ 15, /*DC=*/ 27, /*RST=*/ 26, /*BUSY=*/ 25));
#endif

static union {
  uint8_t efuse_mac[6];
  uint64_t chipmacid;
};

#if defined(SD_CARD) && defined(LILYGO_AMOLED_1_75)
// Dedicated SPI bus for SD card on LilyGo to prevent conflicts
SPIClass SD_SPI(HSPI);
#endif

#if defined(AUDIO)
static uint8_t sdcard_files_to_open = 0;
File wavFile;

// SPIClass uSD_SPI(HSPI);

// uCDB<SDFileSystemClass, SDFile> ucdb(SD);

/* variables hold file, state of process wav file and wav file properties */
wavProperties_t wavProps;

//i2s configuration
int i2s_num = 0; // i2s port number
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 22050,  // will be updated per WAV
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 10,      // Increased from 8 to 10 for better buffering
    .dma_buf_len = 512,       // Increased from 256 to 512 for smoother playback
    .use_apll = true,         // Use audio PLL for better clock accuracy
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

i2s_pin_config_t pin_config = {
    .bck_io_num   = SOC_GPIO_PIN_BCLK,
    .ws_io_num    = SOC_GPIO_PIN_LRCLK,
    .data_out_num = SOC_GPIO_PIN_DOUT,
    .data_in_num  = -1  // Not used
};
#endif /* AUDIO */

// RTC_DATA_ATTR int bootCount = 0;

static uint32_t ESP32_getFlashId()
{
  return g_rom_flashchip.device_id;
}

void ESP32_TFT_fini(const char *msg)
{
  if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_BLUEBUTTON, TFT_BLACK);
    sprite.setFreeFont(&Orbitron_Light_32);
    uint16_t wd = sprite.textWidth(msg);

    sprite.setCursor(LCD_WIDTH / 2 - wd /2, LCD_HEIGHT /2 + 20);
    sprite.printf(msg);
  
    lcd_brightness(255);
    lcd_PushColors(display_column_offset, 0, 466, 466, (uint16_t*)sprite.getPointer());
    delay(2000);
    lcd_brightness(0);
    lcd_sleep();
    xSemaphoreGive(spiMutex);
  } else {
    Serial.println("Failed to acquire SPI semaphore!");
  }
}

void ESP32_fini()
{
  Serial.println("Putting device to deep sleep...");
  delay(1000);
  BuddyManager::clearBuddyList();
  EEPROM_store();
  delay(1000);

  // Shutdown peripherals to minimize power consumption

  // Isolate GPIO pads to prevent leakage current in deep sleep
  // This prevents the green screen by properly powering down the display
#if defined(LILYGO_AMOLED_1_75)
  rtc_gpio_isolate(GPIO_NUM_16);  // LCD_EN for LilyGo
  rtc_gpio_isolate(GPIO_NUM_8);   // TOUCH_RST for LilyGo
#elif defined(WAVESHARE_AMOLED_1_75)
  rtc_gpio_isolate(GPIO_NUM_13);  // LCD_VCI_EN for Waveshare
  rtc_gpio_isolate(GPIO_NUM_40);  // TOUCH_RST for Waveshare
#endif

  // Configure GPIO0 BEFORE disabling wake sources
  gpio_hold_en(GPIO_NUM_0);

  // Disable all wake sources, then enable only GPIO0
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);

  // Close communication buses
  SPI.end();

#if defined (XPOWERS_CHIP_AXP2101)
  // 5. Configure PMU for deep sleep
  prepare_AXP2101_deep_sleep();
#endif

  // IMPORTANT: Do NOT use Serial.flush() as it blocks when USB CDC is not connected!
  // Just print and give a short delay for the message to be sent
  Serial.println("Entering deep sleep now...");
  delay(100);  // Brief delay to allow serial TX buffer to empty

  // Note: Serial is NOT closed - it will be automatically handled by deep sleep

  esp_deep_sleep_start();
}

static void ESP32_setup()
{
  pinMode(GPIO_NUM_0, INPUT);
  //Check if the WAKE reason was button press
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    if (digitalRead(GPIO_NUM_0) == LOW) {
      // Only proceed if button is still pressed (i.e., held)
      unsigned long start = millis();
      while (digitalRead(GPIO_NUM_0) == LOW) {
        if (millis() - start >= 1000) {
          break;
        }
      }
      // If released before timeout, go back to sleep
      if (millis() - start < 1000) {
      // Wait for button to be released
        while (digitalRead(GPIO_NUM_0) == LOW) {
          delay(10);  // avoid tight loop
        }
        
        gpio_hold_en(GPIO_NUM_0);
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
        esp_deep_sleep_start();
      }

    } else {
      // Pin is HIGH, accidental wakeup
      delay(1000);  // give some time before going back to sleep
      gpio_hold_en(GPIO_NUM_0);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
      esp_deep_sleep_start();
    }
  }
  esp_err_t ret = ESP_OK;
  uint8_t null_mac[6] = {0};

//  ++bootCount;

  ret = esp_efuse_mac_get_custom(efuse_mac);
  if (ret != ESP_OK) {
      // ESP_LOGE(TAG, "Get base MAC address from BLK3 of EFUSE error (%s)", esp_err_to_name(ret));
    /* If get custom base MAC address error, the application developer can decide what to do:
     * abort or use the default base MAC address which is stored in BLK0 of EFUSE by doing
     * nothing.
     */

    // ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
    chipmacid = ESP.getEfuseMac();
  } else {
    if (memcmp(efuse_mac, null_mac, 6) == 0) {
      // ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
      chipmacid = ESP.getEfuseMac();
    }
  }

  uint32_t flash_id = ESP32_getFlashId();

  /*
   *    Board         |   Module   |  Flash memory IC
   *  ----------------+------------+--------------------
   *  DoIt ESP32      | WROOM      | GIGADEVICE_GD25Q32
   *  TTGO T3  V2.0   | PICO-D4 IC | GIGADEVICE_GD25Q32
   *  TTGO T3  V2.1.6 | PICO-D4 IC | GIGADEVICE_GD25Q32
   *  TTGO T22 V06    |            | WINBOND_NEX_W25Q32_V
   *  TTGO T22 V08    |            | WINBOND_NEX_W25Q32_V
   *  TTGO T22 V11    |            | BOYA_BY25Q32AL
   *  TTGO T8  V1.8   | WROVER     | GIGADEVICE_GD25LQ32
   *  TTGO T5S V1.9   |            | WINBOND_NEX_W25Q32_V
   *  TTGO T5S V2.8   |            | BOYA_BY25Q32AL
   *  TTGO T5  4.7    | WROVER-E   | XMC_XM25QH128C
   *  TTGO T-Watch    |            | WINBOND_NEX_W25Q128_V
   */

  if (psramFound()) {
         // Check if PSRAM is available

    hw_info.revision = HW_REV_H741_01;
    // switch(flash_id)
    // {
    // case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25LQ32):
    //   /* ESP32-WROVER module */
    //   hw_info.revision = HW_REV_T8_1_8;
    //   break;
    // case MakeFlashId(ST_ID, XMC_XM25QH128C):
    //   /* custom ESP32-WROVER-E module with 16 MB flash */
    //   hw_info.revision = HW_REV_T5_1;
    //   break;
    // default:
    //   hw_info.revision = HW_REV_UNKNOWN;
    //   break;
    // }
  } else {
    Serial.println("PSRAM is NOT enabled or not available!");
    // switch(flash_id)
    // {
    // case MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25Q32):
    //   hw_info.revision = HW_REV_DEVKIT;
    //   break;
    // case MakeFlashId(WINBOND_NEX_ID, WINBOND_NEX_W25Q32_V):
    //   hw_info.revision = HW_REV_T5S_1_9;
    //   break;
    // case MakeFlashId(BOYA_ID, BOYA_BY25Q32AL):
    //   hw_info.revision = HW_REV_T5S_2_8;
    //   break;
    // default:
    //   hw_info.revision = HW_REV_UNKNOWN;
    //   break;
    // }
  }
  Serial.println(hw_info.revision);

  // Initialize button BEFORE SD card to ensure GPIO 0 pull-up is active during SD init
#if defined(BUTTONS)
  Serial.println("Initializing button before SD card...");
  ESP32_Button_setup();
  delay(100);  // Allow button pull-ups to stabilize
#endif

  // Initialise SD card.
#if defined(SD_CARD)
  // Initialize dedicated SPI bus for SD card to prevent conflicts with other peripherals
  // LilyGo AMOLED uses SD card on separate SPI pins (38-41)
  Serial.println("Initializing SD card on dedicated SPI bus...");

#if defined(LILYGO_AMOLED_1_75)
  SD_SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
#else
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
#endif

  delay(100);  // Give SD card time to power up

  // Try to begin SD with reduced frequency (4MHz) for better stability
  const uint32_t SD_INIT_FREQ = 4000000;  // 4MHz for initialization

#if defined(LILYGO_AMOLED_1_75)
  if (!SD.begin(SD_CS, SD_SPI, SD_INIT_FREQ)) {
#else
  if (!SD.begin(SD_CS, SPI, SD_INIT_FREQ)) {
#endif
      Serial.println("Card Mount Failed!");
      return;
  }

  delay(50);  // Allow SD card to stabilize after mount

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
      Serial.println("No SD card attached!");
      return;
  }
  else {
      Serial.println("SD card detected.");
      Serial.print("SD Card Type: ");
      switch (cardType) {
          case CARD_SD:
              Serial.println("SD");
              break;
          case CARD_SDHC:
              Serial.println("SDHC");
              break;

          default:
              Serial.println("Unknown");
              break;
      }
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      Serial.printf("SD Card Size: %lluMB\n", cardSize);
  }

  Serial.println("SD card initialized.");
#endif //SD_CARD

#if defined(AUDIO)
  // Initialize amplifier enable pin
  pinMode(SOC_GPIO_AMP_ENABLE, OUTPUT);
  digitalWrite(SOC_GPIO_AMP_ENABLE, LOW);  // Start with amplifier OFF
  Serial.println("Audio amplifier pin initialized (GPIO 47)");
#endif
}

static uint32_t ESP32_getChipId()
{
  return (uint32_t) efuse_mac[5]        | (efuse_mac[4] << 8) | \
                   (efuse_mac[3] << 16) | (efuse_mac[2] << 24);
}

static bool ESP32_EEPROM_begin(size_t size)
{
  return EEPROM.begin(size);
}

static const int8_t ESP32_dB_to_power_level[21] = {
  8,  /* 2    dB, #0 */
  8,  /* 2    dB, #1 */
  8,  /* 2    dB, #2 */
  8,  /* 2    dB, #3 */
  8,  /* 2    dB, #4 */
  20, /* 5    dB, #5 */
  20, /* 5    dB, #6 */
  28, /* 7    dB, #7 */
  28, /* 7    dB, #8 */
  34, /* 8.5  dB, #9 */
  34, /* 8.5  dB, #10 */
  44, /* 11   dB, #11 */
  44, /* 11   dB, #12 */
  52, /* 13   dB, #13 */
  52, /* 13   dB, #14 */
  60, /* 15   dB, #15 */
  60, /* 15   dB, #16 */
  68, /* 17   dB, #17 */
  74, /* 18.5 dB, #18 */
  76, /* 19   dB, #19 */
  78  /* 19.5 dB, #20 */
};

static void ESP32_WiFi_setOutputPower(int dB)
{
  if (dB > 20) {
    dB = 20;
  }

  if (dB < 0) {
    dB = 0;
  }

  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(ESP32_dB_to_power_level[dB]));
}

static bool ESP32_WiFi_hostname(String aHostname)
{
  return WiFi.setHostname(aHostname.c_str());
}

static void ESP32_swSer_begin(unsigned long baud)
{
  SerialInput.begin(baud, SERIAL_8N1, SOC_GPIO_PIN_GNSS_RX, SOC_GPIO_PIN_GNSS_TX);
  SerialInput.setRxBufferSize(baud / 10); /* 1 second */
}

static void ESP32_swSer_enableRx(boolean arg)
{

}

static uint32_t ESP32_maxSketchSpace()
{
  return 0x3F0000;
}

static void ESP32_WiFiUDP_stopAll()
{
/* not implemented yet */
}

static void ESP32_Battery_setup()
{
  PMU_setup();
}

static float ESP32_Battery_voltage()
{
  // Serial.println("Reading battery voltage from SY6970...");
  delay(100); // Give some time for the SY6970 to stabilize
  float voltage = read_PMU_voltage() * 0.001;
  Serial.printf("Battery voltage: %.2fV\n", voltage);
  return voltage;

}


#if defined(USE_EPAPER)
static portMUX_TYPE EPD_ident_mutex;

//static ep_model_id ESP32_EPD_ident()
static int ESP32_EPD_ident()
{
  // ep_model_id rval = EP_GDEW027W3; /* default */
  int rval = EP_GDEW027W3; /* default */
#if 0
  vPortCPUInitializeMutex(&EPD_ident_mutex);

  digitalWrite(SOC_GPIO_PIN_SS_T5S, HIGH);
  pinMode(SOC_GPIO_PIN_SS_T5S, OUTPUT);
  digitalWrite(SOC_EPD_PIN_DC_T5S, HIGH);
  pinMode(SOC_EPD_PIN_DC_T5S, OUTPUT);

  digitalWrite(SOC_EPD_PIN_RST_T5S, LOW);
  pinMode(SOC_EPD_PIN_RST_T5S, OUTPUT);
  delay(20);
  pinMode(SOC_EPD_PIN_RST_T5S, INPUT_PULLUP);
  delay(200);
  pinMode(SOC_EPD_PIN_BUSY_T5S, INPUT);

  swSPI.begin();

  taskENTER_CRITICAL(&EPD_ident_mutex);

  digitalWrite(SOC_EPD_PIN_DC_T5S,  LOW);
  digitalWrite(SOC_GPIO_PIN_SS_T5S, LOW);

  swSPI.transfer_out(0x71);

  pinMode(SOC_GPIO_PIN_MOSI_T5S, INPUT);
  digitalWrite(SOC_EPD_PIN_DC_T5S, HIGH);

  uint8_t status = swSPI.transfer_in();

  digitalWrite(SOC_GPIO_PIN_SCK_T5S, LOW);
  digitalWrite(SOC_EPD_PIN_DC_T5S,  LOW);
  digitalWrite(SOC_GPIO_PIN_SS_T5S,  HIGH);

  taskEXIT_CRITICAL(&EPD_ident_mutex);

  swSPI.end();

//#if 0
//  Serial.print("REG 71H: ");
//  Serial.println(status, HEX);
//#endif

//  if (status != 2) {
//    rval = EP_GDEY027T91; /* TBD */
//  }
#endif
  return rval;
}

#define EPD_STACK_SZ      (256*4)
static TaskHandle_t EPD_Task_Handle = NULL;

//static ep_model_id ESP32_display = EP_UNKNOWN;
static int ESP32_display = EP_UNKNOWN;

static void ESP32_EPD_setup()
{
  switch(settings->adapter)
  {
  case ADAPTER_WAVESHARE_ESP32:
    display = &epd_waveshare_W3;
//    display = &epd_waveshare_T91;
    SPI.begin(SOC_GPIO_PIN_SCK_WS,
              SOC_GPIO_PIN_MISO_WS,
              SOC_GPIO_PIN_MOSI_WS,
              SOC_GPIO_PIN_SS_WS);
    break;
#if defined(BUILD_SKYVIEW_HD)
  case ADAPTER_TTGO_T5_4_7:
    display = NULL;
    break;
#endif /* BUILD_SKYVIEW_HD */
  case ADAPTER_TTGO_T5S:
  default:
    if (ESP32_display == EP_UNKNOWN) {
      ESP32_display = ESP32_EPD_ident();
    }

    switch (ESP32_display)
    {
    // case EP_GDEY027T91:
    //   display = &epd_ttgo_t5s_T91;
    //   break;
    case EP_GDEW027W3:
    // default:
      display = &epd_ttgo_t5s_W3;
      break;
    }
    Serial.println("SPI begin");
    SPI.begin(SOC_GPIO_PIN_SCK_T5S,
              SOC_GPIO_PIN_MISO_T5S,
              SOC_GPIO_PIN_MOSI_T5S,
              SOC_GPIO_PIN_SS_T5S);

    /* SD-SPI init */
    uSD_SPI.begin(SOC_SD_PIN_SCK_T5S,
                  SOC_SD_PIN_MISO_T5S,
                  SOC_SD_PIN_MOSI_T5S,
                  SOC_SD_PIN_SS_T5S);
    break;
  }

  xTaskCreateUniversal(EPD_Task, "EPD update", EPD_STACK_SZ, NULL, 1,
                       &EPD_Task_Handle, CONFIG_ARDUINO_RUNNING_CORE);
}

static void ESP32_EPD_fini()
{
  if( EPD_Task_Handle != NULL )
  {
    vTaskDelete( EPD_Task_Handle );
  }
}

static bool ESP32_EPD_is_ready()
{
//  return true;
  return (EPD_task_command == EPD_UPDATE_NONE);
}

static void ESP32_EPD_update(int val)
{
//  EPD_Update_Sync(val);
  EPD_task_command = val;
}
#endif /* USE_EPAPER */

static size_t ESP32_WiFi_Receive_UDP(uint8_t *buf, size_t max_size)
{
  return WiFi_Receive_UDP(buf, max_size);
}

static IPAddress ESP32_WiFi_get_broadcast()
{
  tcpip_adapter_ip_info_t info;
  IPAddress broadcastIp;

  if (WiFi.getMode() == WIFI_STA) {
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &info);
  } else {
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &info);
  }
  broadcastIp = ~info.netmask.addr | info.ip.addr;

  return broadcastIp;
}

static void ESP32_WiFi_Transmit_UDP(int port, byte *buf, size_t size)
{
  IPAddress ClientIP;
  WiFiMode_t mode = WiFi.getMode();
  int i = 0;

  switch (mode)
  {
  case WIFI_STA:
    ClientIP = ESP32_WiFi_get_broadcast();

    Uni_Udp.beginPacket(ClientIP, port);
    Uni_Udp.write(buf, size);
    Uni_Udp.endPacket();

    break;
  case WIFI_AP:
    wifi_sta_list_t stations;
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&stations));

    tcpip_adapter_sta_list_t infoList;
    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stations, &infoList));

    while(i < infoList.num) {
      ClientIP = infoList.sta[i++].ip.addr;

      Uni_Udp.beginPacket(ClientIP, port);
      Uni_Udp.write(buf, size);
      Uni_Udp.endPacket();
    }
    break;
  case WIFI_OFF:
  default:
    break;
  }
}

static int ESP32_WiFi_clients_count()
{
  WiFiMode_t mode = WiFi.getMode();

  switch (mode)
  {
  case WIFI_AP:
    wifi_sta_list_t stations;
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&stations));

    tcpip_adapter_sta_list_t infoList;
    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stations, &infoList));

    return infoList.num;
  case WIFI_STA:
  default:
    return -1; /* error */
  }
}
#if !defined(BUILD_SKYVIEW_HD) && !defined(ESP32S3)
static bool SD_is_ok = false;
static bool ADB_is_open = false;

static bool ESP32_DB_init()
{
  bool rval = false;

  if (settings->adapter != ADAPTER_TTGO_T5S) {
    return rval;
  }



  sdcard_files_to_open += (settings->adb   == DB_FLN    ? 1 : 0);
  sdcard_files_to_open += (settings->adb   == DB_OGN    ? 1 : 0);
  sdcard_files_to_open += (settings->adb   == DB_ICAO   ? 1 : 0);
  sdcard_files_to_open += (settings->voice != VOICE_OFF ? 2 : 0);   // we use voice1 & voice3

  if (!SD.begin(SOC_SD_PIN_SS_T5S, uSD_SPI, 4000000, "/sd", sdcard_files_to_open)) {
    Serial.println(F("ERROR: Failed to mount microSD card."));
    return rval;
  }

  if (SD.cardType() == CARD_NONE)
    return rval;

  SD_is_ok = true;

  if (settings->adb == DB_NONE) {
    return rval;
  }

  if (settings->adb == DB_FLN) {
    if (ucdb.open("/Aircrafts/fln.cdb") == CDB_OK) {
      Serial.print("FLN records: ");
      Serial.println(ucdb.recordsNumber());
      rval = true;
    } else {
      Serial.println(F("Failed to open FlarmNet DB\n"));
    }
  }

  if (settings->adb == DB_OGN) {
    if (ucdb.open("/Aircrafts/ogn.cdb") == CDB_OK) {
      Serial.print("OGN records: ");
      Serial.println(ucdb.recordsNumber());
      rval = true;
    } else {
      Serial.println(F("Failed to open OGN DB\n"));
    }
  }

  if (settings->adb == DB_ICAO) {
    if (ucdb.open("/Aircrafts/icao.cdb") == CDB_OK) {
      Serial.print("ICAO records: ");
      Serial.println(ucdb.recordsNumber());
      rval = true;
    } else {
      Serial.println(F("Failed to open ICAO DB\n"));
    }
  }


  if (rval)
    ADB_is_open = true;

  return rval;
}

static int ESP32_DB_query(uint8_t type, uint32_t id, char *buf, size_t size,
                            char *buf2=NULL, size_t size2=0)
{
  // The 'type' argument is for selecting which DB (OGN, FLN, etc).
  // For now we ignore it.  Only ogn.cdb is supported.

  char key[8];
  char out[64];
  uint8_t tokens[4] = { 0 };   // was [3] - allow room for future inclusion of aircraft type
  cdbResult rt;
  int c, i = 0;
  int token_cnt = 1;           // token[0] always exists, preset to index 0
  int nothing;

  if (!SD_is_ok)
    return -2;   // no SD card

  if (!ADB_is_open)
    return -1;   // no database

  snprintf(key, sizeof(key),"%06X", id);

  rt = ucdb.findKey(key, strlen(key));

  if (rt == KEY_FOUND) {
      while ((c = ucdb.readValue()) != -1 && i < (sizeof(out) - 1)) {
        if (c == '|') {
          if (token_cnt < sizeof(tokens)) {
            token_cnt++;
            tokens[token_cnt-1] = i+1;     // start of NEXT token
          }
          c = '\0';    // null-terminate the previous token
        }
        out[i++] = (char) c;
      }
      out[i] = '\0';    // null-terminate the last token
      nothing = i;      // index of an empty string
      if (token_cnt < 2)  tokens[1] = nothing;
      if (token_cnt < 3)  tokens[2] = nothing;
      if (token_cnt < 4)  tokens[3] = nothing;

      // this code is specific to ogn.cdb
      // if we ever use fln.cdb need specific code for that

      int pref1, pref2, pref3;
      switch (settings->idpref)
      {
      case ID_TAIL:
        pref1 = tokens[2];   // CN
        pref2 = tokens[0];   // M&M
        pref3 = tokens[1];   // reg
        break;
      case ID_MAM:
        pref1 = tokens[0];
        pref2 = tokens[1];
        pref3 = tokens[2];
        break;
      case ID_REG:
        pref1 = tokens[1];
        pref2 = tokens[0];
        pref3 = tokens[2];
        break;
      default:
        pref1 = nothing;
        pref2 = nothing;
        pref3 = nothing;
        break;
      }
      // try and show BOTH first and second preference
      // data fields, e.g., contest number AND M&M:
#if 0
// single line
      if (buf2) buf2[0] = '\0';
      if (strlen(out + pref1)) {
        snprintf(buf, size, "%s:%s",
          out + pref1,
          (strlen(out + pref2) ? out + pref2 :
           strlen(out + pref3) ? out + pref3 : ""))
      } else if (strlen(out + pref2)) {
        snprintf(buf, size, "%s:%s",
          out + pref2,
          (strlen(out + pref3) ? out + pref3 : ""))
      } else if (strlen(out + pref3)) {
        snprintf(buf, size, "%s", out + pref3);
      } else {
        buf[0] = '\0';
        return 2;   // found, but empty record
      }
#else
// will be two lines on the display
      if (out[pref1]) {
        snprintf(buf, size, "%s", &out[pref1]);
        if (buf2)
          snprintf(buf2, size2, "%s",
            (out[pref2] ? &out[pref2] :
             out[pref3] ? &out[pref3] : ""));
      } else if (out[pref2]) {
        snprintf(buf, size, "%s", &out[pref2]);
        if (buf2)
          snprintf(buf2, size2, "%s",
            (out[pref3] ? &out[pref3] : ""));
      } else if (out[pref3]) {
        snprintf(buf, size, "%s", &out[pref3]);
        if (buf2)  buf2[0] = '\0';
      } else {
        buf[0] = '\0';
        if (buf2)  buf2[0] = '\0';
        return 2;   // found, but empty record
      }
#endif
      return 1;  // found
  }

  return 0;   // not found
}

static void ESP32_DB_fini()
{
#if !defined(BUILD_SKYVIEW_HD)
  if (settings->adapter == ADAPTER_TTGO_T5S) {

    if (ADB_is_open) {
      ucdb.close();
      ADB_is_open = false;
    }

    SD.end();
    SD_is_ok = false;
  }
}
#endif /* BUILD_SKYVIEW_HD */
#endif
#if defined(AUDIO)
/* write sample data to I2S */
int i2s_write_sample_nb(uint32_t sample)
{
#if 1   // ESP_IDF_VERSION_MAJOR>=4
    size_t i2s_bytes_written;
    i2s_write((i2s_port_t)i2s_num, (const char*)&sample, sizeof(uint32_t), &i2s_bytes_written, 100);
    return i2s_bytes_written;
#else
  return i2s_write_bytes((i2s_port_t)i2s_num, (const char *)&sample,
                          sizeof(uint32_t), 100);
#endif
}

/* read 4 bytes of data from wav file */
int read4bytes(File file, uint32_t *chunkId)
{
  int n = file.read((uint8_t *)chunkId, sizeof(uint32_t));
  return n;
}

/* these are functions to process wav file */
int readRiff(File file, wavRiff_t *wavRiff)
{
  int n = file.read((uint8_t *)wavRiff, sizeof(wavRiff_t));
  return n;
}
int readProps(File file, wavProperties_t *wavProps)
{
  int n = file.read((uint8_t *)wavProps, sizeof(wavProperties_t));
  return n;
}

static bool play_file(char *filename, int volume)
{
    headerState_t state = HEADER_RIFF;

    // Try to open the file directly without checking cardType()
    // cardType() can trigger unwanted remount attempts
    // SPI bus should be initialized once per TTS call, not per file
    File wavfile = SD.open(filename);
    if (! wavfile) {
      Serial.print(F("Error opening WAV file: "));
      Serial.println(filename);
      return false;
    } else {
      Serial.print(F("Playing WAV file: "));
      Serial.println(filename);
    }

    int c = 0;
    int n;
    while (wavfile.available()) {
      switch(state){
        case HEADER_RIFF:
        wavRiff_t wavRiff;
        n = readRiff(wavfile, &wavRiff);
        if(n == sizeof(wavRiff_t)){
          if(wavRiff.chunkID == CCCC('R', 'I', 'F', 'F') && wavRiff.format == CCCC('W', 'A', 'V', 'E')){
            state = HEADER_FMT;
//            Serial.println("HEADER_RIFF");
          }
        }
        break;
        case HEADER_FMT:
        n = readProps(wavfile, &wavProps);
        if(n == sizeof(wavProperties_t)){
          state = HEADER_DATA;
#if 0
            Serial.print("chunkID = "); Serial.println(wavProps.chunkID);
            Serial.print("chunkSize = "); Serial.println(wavProps.chunkSize);
            Serial.print("audioFormat = "); Serial.println(wavProps.audioFormat);
            Serial.print("numChannels = "); Serial.println(wavProps.numChannels);
            Serial.print("sampleRate = "); Serial.println(wavProps.sampleRate);
            Serial.print("byteRate = "); Serial.println(wavProps.byteRate);
            Serial.print("blockAlign = "); Serial.println(wavProps.blockAlign);
            Serial.print("bitsPerSample = "); Serial.println(wavProps.bitsPerSample);
#endif
        }
        break;
        case HEADER_DATA:
        uint32_t chunkId, chunkSize;
        n = read4bytes(wavfile, &chunkId);
        if(n == 4){
          if(chunkId == CCCC('d', 'a', 't', 'a')){
//            Serial.println("HEADER_DATA");
          }
        }
        n = read4bytes(wavfile, &chunkSize);
        if(n == 4){
//          Serial.println("prepare data");
          state = DATA;
        }
        i2s_driver_install((i2s_port_t)i2s_num, &i2s_config, 0, NULL);
        i2s_set_pin((i2s_port_t)i2s_num, &pin_config);
        i2s_zero_dma_buffer((i2s_port_t)i2s_num);  // Clear DMA buffer to prevent noise

        // CRITICAL: Re-configure GPIO0 after I2S pin setup
        // I2S driver may disable pull-ups on nearby GPIO pins
        gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
        gpio_set_pull_mode(GPIO_NUM_0, GPIO_PULLUP_ONLY);

        //set sample rates of i2s to sample rate of wav file
        i2s_set_sample_rates((i2s_port_t)i2s_num, wavProps.sampleRate);
        break;
        /* after processing wav file, it is time to process music data */
        case DATA:
        // Use buffered reading for smooth playback (like working demo)
        uint8_t audioBuffer[1024];
        size_t bytesRead, bytesWritten;

        while (wavfile.available()) {
            bytesRead = wavfile.read(audioBuffer, sizeof(audioBuffer));
            if (bytesRead > 0) {
                // Write buffered data directly to I2S - no volume adjustment needed
                i2s_write((i2s_port_t)i2s_num, audioBuffer, bytesRead, &bytesWritten, portMAX_DELAY);
            }
        }
        break;
      }
    }

    wavfile.close();
    if (state == DATA) {
      i2s_driver_uninstall((i2s_port_t)i2s_num); //stop & destroy i2s driver

      // CRITICAL: Restore GPIO0 pull-up after I2S driver uninstall
      gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
      gpio_set_pull_mode(GPIO_NUM_0, GPIO_PULLUP_ONLY);
    }

    return true;
}

static void ESP32_TTS(char *message)
{
    char filename[MAX_FILENAME_LEN];

    if (settings->voice == VOICE_OFF)
      return;

#if !defined(SD_CARD)
    // Audio requires SD card support
    Serial.println(F("Audio requires SD_CARD to be defined"));
    return;
#endif

#if defined(SD_CARD) && defined(LILYGO_AMOLED_1_75)
    // Reinitialize SD SPI bus before first access
    // This ensures SPI pins are correctly configured after WiFi/display init
    Serial.println("Reinitializing SD SPI bus for audio playback...");
    SD_SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    delay(50);
#endif

    // Enable audio amplifier
    Serial.println("Enabling audio amplifier...");
    digitalWrite(SOC_GPIO_AMP_ENABLE, HIGH);
    delay(200);  // Give amplifier time to power up

    if (strcmp(message, "POST")) {   // *not* the post-booting demo
      // Don't call SD.cardType() here - it can trigger unwanted remount attempts
      // Instead, just try to access files and handle errors in play_file()
#if defined(USE_EPAPER)
      while (!SoC->EPD_is_ready()) {yield();}
      EPD_Message("VOICE", "ALERT");
      SoC->EPD_update(EPD_UPDATE_FAST);
      while (!SoC->EPD_is_ready()) {yield();}
#endif /* USE_EPAPER */
      bool wdt_status = loopTaskWDTEnabled;

      if (wdt_status) {
        disableLoopWDT();
      }

      char *word = strtok (message, " ");

      while (word != NULL)
      {
          strcpy(filename, WAV_FILE_PREFIX);
          strcat(filename, settings->voice == VOICE_1 ? VOICE1_SUBDIR :
                          (settings->voice == VOICE_2 ? VOICE2_SUBDIR :
                          (settings->voice == VOICE_3 ? VOICE3_SUBDIR :
                           "" )));
          strcat(filename, word);
          strcat(filename, WAV_FILE_SUFFIX);
          // voice_3 in the existing collection of .wav files is quieter than voice_1,
          // so make it a bit louder since we use it for more-urgent advisories
          int volume = (settings->voice == VOICE_3)? 1 : 0;
          play_file(filename, volume);
          word = strtok (NULL, " ");

          yield();

          /* Poll input source(s) */
          Input_loop();
      }

      if (wdt_status) {
        enableLoopWDT();
      }

      // Disable amplifier after playback
      digitalWrite(SOC_GPIO_AMP_ENABLE, LOW);
      Serial.println("Audio amplifier disabled.");

   } else {   /* post-booting */
      // Don't call SD.cardType() - let play_file() handle file access errors
      // SD card should be ready from initialization in ESP32_setup()

      //Start-up tones
      /* demonstrate the voice output */
      delay(1500);
      settings->voice = VOICE_1;
      strcpy(filename, WAV_FILE_PREFIX);
      strcat(filename, VOICE1_SUBDIR);
      strcat(filename, "notice");
      strcat(filename, WAV_FILE_SUFFIX);
      if (!play_file(filename, 0)) {
        Serial.println(F("POST: Failed to play voice1 notice.wav - check SD card and files"));
      }
      delay(1500);
      settings->voice = VOICE_2;
      strcpy(filename, WAV_FILE_PREFIX);
      strcat(filename, VOICE2_SUBDIR);
      strcat(filename, "notice");
      strcat(filename, WAV_FILE_SUFFIX);
      if (!play_file(filename, 0)) {   // No volume adjustment needed
        Serial.println(F("POST: Failed to play voice2 notice.wav - check SD card and files"));
      }
      delay(1000);

      // Disable amplifier after POST audio
      digitalWrite(SOC_GPIO_AMP_ENABLE, LOW);
      Serial.println("POST audio complete. Amplifier disabled.");
    }
}
#endif /* AUDIO */
#if defined(BUTTONS)

#include <AceButton.h>
using namespace ace_button;

AceButton button_mode(BUTTON_MODE_PIN);
// AceButton button_up  (SOC_BUTTON_UP_T5S);
// AceButton button_down(SOC_BUTTON_DOWN_T5S);

// Track if long press just opened the menu
static bool justOpenedMenu = false;

// Button task handle
TaskHandle_t buttonTaskHandle = NULL;

// Forward declaration
void buttonTask(void *parameter);

// The event handler for the button.
void handleEvent(AceButton* button, uint8_t eventType,
    uint8_t buttonState) {

  switch (eventType) {
    case AceButton::kEventPressed:
      // Button pressed - no action
      break;

    case AceButton::kEventReleased:
      // With suppress features, RELEASED acts as single click (immediate)
      // CLICKED event comes later if no double-click detected
      if (button == &button_mode) {
        // Clear the flag after any release
        justOpenedMenu = false;

        // Don't handle as click here - let CLICKED handle it after delay
      }
      break;

    case AceButton::kEventClicked:
      // This fires after double-click timeout if it was just a single click
      if (button == &button_mode) {
        if (TFT_view_mode == VIEW_MODE_POWER) {
          // Single click when menu is showing = Sleep
          shutdown("SLEEP");
        } else {
          // Single click when menu not showing = Change mode
          TFT_Mode(true);
        }
      }
      break;

    case AceButton::kEventDoubleClicked:
      if (button == &button_mode) {
      // Double click when menu not showing = Restore previous view
          TFT_DoubleClick();
      }
      break;

    case AceButton::kEventLongPressed:
      if (button == &button_mode) {
        // Long press - show power options menu
        justOpenedMenu = true;  // Set flag to ignore the release after long press
        TFT_show_power_menu();
      }
      break;
  }
}
  

/* Callbacks for push button interrupt */
// void onModeButtonEvent() {
//   button_mode.check();
// }

// void onUpButtonEvent() {
//   // button_up.check();
// }

// void onDownButtonEvent() {
//   // button_down.check();
// }

static void ESP32_Button_setup()
{
  // Prevent double initialization - can be called multiple times safely
  static bool button_initialized = false;
  if (button_initialized) {
    Serial.println("Button already initialized, skipping duplicate setup");
    return;
  }

  int mode_button_pin = BUTTON_MODE_PIN;

  // Button(s) uses internal pull up resistor.
  pinMode(mode_button_pin, INPUT_PULLUP);

  // Configure GPIO 0 with strong pull-up BEFORE any SD card or audio operations
  pinMode(GPIO_NUM_0, INPUT_PULLUP);
  gpio_pullup_en(GPIO_NUM_0);
  gpio_pulldown_dis(GPIO_NUM_0);

  Serial.println("GPIO 0 configured with strong pull-up before button init");

  button_mode.init(mode_button_pin, HIGH, 0);  // Active LOW button (pressed = LOW)

  // Configure the ButtonConfig with the event handler, and enable all higher
  // level events.
  ButtonConfig* ModeButtonConfig = button_mode.getButtonConfig();
  ModeButtonConfig->setEventHandler(handleEvent);

  // Enable features - following AceButton example for DoubleClick
  ModeButtonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  ModeButtonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  ModeButtonConfig->setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
  ModeButtonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
  ModeButtonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);

  // Set timing parameters
  // Increased debounce delay when SD_CARD and AUDIO enabled to filter noise
#if defined(SD_CARD) && defined(AUDIO)
  ModeButtonConfig->setDebounceDelay(50);  // Increased from 15ms to filter SPI noise
#else
  ModeButtonConfig->setDebounceDelay(15);
#endif
  ModeButtonConfig->setClickDelay(300);
  ModeButtonConfig->setDoubleClickDelay(400);
  ModeButtonConfig->setLongPressDelay(2000);

  // Create dedicated button task pinned to core 1 (same as touch task)
  xTaskCreatePinnedToCore(buttonTask, "Button Task", 4096, NULL, 1, &buttonTaskHandle, 1);

  // Mark as initialized to prevent duplicate setup
  button_initialized = true;
  Serial.println("Button initialization complete");
}

// Button task - runs continuously to check button state
void buttonTask(void *parameter) {
  // Wait 2 seconds after boot before monitoring buttons
  // This prevents false triggers during system initialization
  // Button state is now cleared during setup, so shorter delay is safe
  vTaskDelay(pdMS_TO_TICKS(2000));

  while(true) {
    button_mode.check();
    vTaskDelay(pdMS_TO_TICKS(5)); // Check every 5ms for very responsive detection
  }
}

static void ESP32_Button_loop()
{
  // Button checking now happens in dedicated task
  // This function kept for compatibility but does nothing
}

static void ESP32_Button_fini()
{

}
#endif //BUTTONS

static void ESP32_WDT_setup()
{
  enableLoopWDT();
}

static void ESP32_WDT_fini()
{
  disableLoopWDT();
}

const SoC_ops_t ESP32_ops = {
  SOC_ESP32,
  "ESP32",
  ESP32_setup,
  ESP32_fini,
  ESP32_getChipId,
  ESP32_getFlashId,
  ESP32_EEPROM_begin,
  ESP32_WiFi_setOutputPower,
  ESP32_WiFi_hostname,
  ESP32_swSer_begin,
  ESP32_swSer_enableRx,
  ESP32_maxSketchSpace,
  ESP32_WiFiUDP_stopAll,
  ESP32_Battery_setup,
  ESP32_Battery_voltage,
#if defined(USE_EPAPER)
  ESP32_EPD_setup,
  ESP32_EPD_fini,
  ESP32_EPD_is_ready,
  ESP32_EPD_update,
#endif
  ESP32_WiFi_Receive_UDP,
  ESP32_WiFi_Transmit_UDP,
  ESP32_WiFi_clients_count,
  #if defined(DB)
  ESP32_DB_init,
  ESP32_DB_query,
  ESP32_DB_fini,
  #endif
  #if defined(AUDIO)
  ESP32_TTS,
  #endif
  #if defined(BUTTONS)
  ESP32_Button_setup,
  ESP32_Button_loop,
  ESP32_Button_fini,
  #endif /* BUTTONS */
  ESP32_WDT_setup,
  ESP32_WDT_fini,
  &ESP32_Bluetooth_ops
};

#endif /* ESP32 */
