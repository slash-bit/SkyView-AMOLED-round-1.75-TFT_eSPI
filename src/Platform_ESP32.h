/*
 * Platform_ESP32.h
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
// #define ARDUINO
#if defined(ESP32)

#ifndef PLATFORM_ESP32_H
#define PLATFORM_ESP32_H

#if defined(ARDUINO)
#include <Arduino.h>
#endif /* ARDUINO */

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

/* Maximum of tracked flying objects is now SoC-specific constant */
#define MAX_TRACKING_OBJECTS  50

#define SerialInput           Serial1
/*HARDWARE SPECIFIC*/
#if defined(AMOLED)
#include "../pins_config.h"
#include "Arduino_DriveBus_Library.h"

#endif /* AMOLED */

/* Peripherals */
#define SOC_GPIO_PIN_GNSS_RX  36
#define SOC_GPIO_PIN_GNSS_TX  6

#if defined(TFT_ST7789)
/* ESP32 and ST7789 SPI pins mapping */
#define SOC_GPIO_PIN_SS_TFT     5
#define SOC_GPIO_PIN_MOSI_TFT   23
#define SOC_GPIO_PIN_MISO_TFT   12
#define SOC_GPIO_PIN_SCK_TFT    18
#define SOC_GPIO_PIN_DC_TFT     16
#endif /* TFT_ST7789 */
#if 0
#define SOC_BUTTON_MODE_DEF   0

/* TTGO T5 and T5S SPI pins mapping */
#define SOC_GPIO_PIN_MOSI_T5S 23
#define SOC_GPIO_PIN_MISO_T5S 12
#define SOC_GPIO_PIN_SCK_T5S  18
#define SOC_GPIO_PIN_SS_T5S   5



/* Waveshare ESP32 SPI pins mapping */
#define SOC_GPIO_PIN_MOSI_WS  14
#define SOC_GPIO_PIN_MISO_WS  12
#define SOC_GPIO_PIN_SCK_WS   13
#define SOC_GPIO_PIN_SS_WS    15

/* TTGO T5S microSD pins mapping */
#define SOC_SD_PIN_MOSI_T5S   15
#define SOC_SD_PIN_MISO_T5S   2
#define SOC_SD_PIN_SCK_T5S    14
#define SOC_SD_PIN_SS_T5S     13



/* TTGO T5S buttons mapping */
#define SOC_BUTTON_MODE_T5S   26
#define SOC_BUTTON_UP_T5S     22
#define SOC_BUTTON_DOWN_T5S   21


/* TTGO T5S green LED mapping */
// #define SOC_GPIO_PIN_LED_T5S  2
#endif

const uint8_t BUTTON_MODE_PIN = 0;
#if defined(AUDIO)
/* LILYGO I2S-out pins mapping - Moved away from GPIO 0-3 to avoid strapping pin conflicts */
#define SOC_GPIO_PIN_BCLK     2   // I2S Bit Clock
#define SOC_GPIO_PIN_LRCLK    1  // I2S Left/Right Clock
#define SOC_GPIO_PIN_DOUT     3   // I2S Data Out
#define SOC_GPIO_AMP_ENABLE   47   // MAX98357A Amplifier Enable/Shutdown
#endif //audio

// #define BUTTON_MODE_PIN      0
/* Boya Microelectronics Inc. */
#define BOYA_ID               0x68
#define BOYA_BY25Q32AL        0x4016

/* ST / SGS/Thomson / Numonyx / XMC(later acquired by Micron) */
#define ST_ID                 0x20
#define XMC_XM25QH128C        0x4018

#define MakeFlashId(v,d)      ((v  << 16) | d)
#define CCCC(c1, c2, c3, c4)  ((c4 << 24) | (c3 << 16) | (c2 << 8) | c1)

#define MAX_FILENAME_LEN      64
#define WAV_FILE_PREFIX       "/Audio/"

#define POWER_SAVING_WIFI_TIMEOUT 300000UL /* 5 minutes */

/* these are data structures to process wav file */
typedef enum headerState_e {
    HEADER_RIFF, HEADER_FMT, HEADER_DATA, DATA
} headerState_t;

typedef struct wavRiff_s {
    uint32_t chunkID;
    uint32_t chunkSize;
    uint32_t format;
} wavRiff_t;

typedef struct wavProperties_s {
    uint32_t chunkID;
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} wavProperties_t;

extern bool loopTaskWDTEnabled;

extern WebServer server;
void ESP32_fini();
extern void ESP32_TFT_fini(const char *msg);
//#define BUILD_SKYVIEW_HD

extern std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus;

extern bool setupWireIfNeeded(int sda, int scl, int freq = 0U);

#endif /* PLATFORM_ESP32_H */

#endif /* ESP32 */
