#if defined(USE_TFT)
#include "TFT_eSPI.h"
// #include "Free_Fonts.h"
#include "SoCHelper.h"
#include "EEPROMHelper.h"
#include "TrafficHelper.h"
#include "TFTHelper.h"
#include "TouchHelper.h"
#include "WiFiHelper.h"
#include "BatteryHelper.h"
#include "View_Text_TFT.h"
// #include <Adafruit_GFX.h>    // Core graphics library
// #include "Arduino_GFX_Library.h"
#include "Platform_ESP32.h"
#include "Arduino_DriveBus_Library.h"
#include "TouchDrvCST92xx.h"
#include <../pins_config.h>
#include <driver/display/CO5300.h>
#include "power.h"

// #include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
// #include <Adafruit_ST77xx.h> // Hardware-specific library for ST7789

// #include <Fonts/GFXFF/FreeSansBold12pt7b.h>
// #include <Fonts/GFXFF/FreeMonoBold24pt7b.h>


#include "SkyView.h"
int TFT_view_mode = 0;
unsigned long TFTTimeMarker = 0;
bool EPD_display_frontpage = false;

int prev_TFT_view_mode = 0;
int settings_page_number = 1;  // Track which settings page (1 or 2)

extern bool wifi_sta;
extern bool TFTrefresh;



#if defined(AMOLED)



// SPIClass SPI_2(HSPI);

// Arduino_DataBus *bus = new Arduino_ESP32QSPI(
//     LCD_CS /* CS */, LCD_SCLK /* SCK */, LCD_SDIO0 /* SDIO0 */, LCD_SDIO1 /* SDIO1 */,
//     LCD_SDIO2 /* SDIO2 */, LCD_SDIO3 /* SDIO3 */);

#if defined DO0143FAT01
// Arduino_GFX *gfx = new Arduino_SH8601(bus, LCD_RST /* RST */,
//                                       0 /* rotation */, false /* IPS */, LCD_WIDTH, LCD_HEIGHT);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite sprite2 = TFT_eSprite(&tft);

#elif defined H0175Y003AM
xSemaphoreHandle spiMutex;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite sprite2 = TFT_eSprite(&tft);
TFT_eSprite bearingSprite = TFT_eSprite(&tft);
TFT_eSprite batterySprite = TFT_eSprite(&tft);
TFT_eSprite compasSprite = TFT_eSprite(&tft);
TFT_eSprite compas2Sprite = TFT_eSprite(&tft);


#else
#error "Unknown macro definition. Please select the correct macro definition."
#endif

  
#endif

unsigned long drawTime = 0;

float batteryToPercentage(float voltage) {
  if (voltage >= 4.2) return 100.0;
  if (voltage <= 3.0) return 0.0;

  // Extended voltage-percentage mapping
  float voltageLevels[] = {4.2, 4.00, 3.9, 3.8, 3.7, 3.6, 3.5, 3.4, 3.3, 3.2, 3.0};
  float percentages[] = {100, 95, 90, 75, 55, 40, 25, 10, 5, 2, 0};

  for (int i = 0; i < 11; i++) { // Loop through voltage levels
      if (voltage > voltageLevels[i + 1]) {
          // Linear interpolation between two points
          return percentages[i] + ((voltage - voltageLevels[i]) /
              (voltageLevels[i + 1] - voltageLevels[i])) * (percentages[i + 1] - percentages[i]);
      }
  }
  return 0.0; // Fallback
}
void draw_battery() {
    //Battery indicator
    uint16_t battery_x = 295;
    uint16_t battery_y = 35;
    float battery = 0;
    uint8_t batteryPercentage = 0;
    uint16_t batt_color = TFT_CYAN;
      // draw battery symbol
   
    
    battery = Battery_voltage();
    // Serial.print(F(" Battery= "));  Serial.println(battery);
    batteryPercentage = (int)batteryToPercentage(battery);
    // Serial.print(F(" Batterypercentage= "));  Serial.println(batteryPercentage);

    if (battery < 3.65 &&  battery >= 3.5) {
      batt_color = TFT_YELLOW;
    } else if (battery < 3.5) {
      batt_color = TFT_RED;
    } else  {
      batt_color = TFT_CYAN;
    }
    if (charging_status() == 3) { //chargin Done.
      batt_color = TFT_GREEN;
    }
    if (charging_status() == 1 || charging_status() == 2) {
      //draw charging icon
    sprite.fillTriangle(battery_x - 20, battery_y + 10, battery_x - 8, battery_y - 3, battery_x - 12, battery_y + 10, batt_color);
    sprite.fillTriangle(battery_x -4, battery_y + 10, battery_x - 16, battery_y + 23, battery_x - 12, battery_y + 10, batt_color);
    }
    sprite.drawRoundRect(battery_x, battery_y, 32, 20, 3, batt_color);
    sprite.fillRect(battery_x + 32, battery_y + 7, 2, 7, batt_color);
    int fillWidth = (int)(30 * ((float)batteryPercentage / 100));
    // Serial.print(F("Fill width = "));
    // Serial.println(fillWidth);
    sprite.fillRect(battery_x + 2, battery_y + 3, fillWidth, 14, batt_color);

    sprite.setCursor(battery_x, battery_y + 24, 4);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.printf("%d%%", batteryPercentage); // Use %% to print the % character
}

void draw_extBattery() {
  uint8_t extBatteryPerc = getBLEbattery();
  if (extBatteryPerc == 0) {
    return; // No external battery data available
  }
    //Battery indicator
    uint16_t battery_x = 80;
    uint16_t battery_y = 325;
    // uint8_t extBatteryPerc = 75;
    uint16_t batt_color = TFT_CYAN;

    if (extBatteryPerc < 50 &&  extBatteryPerc >= 25) {
      batt_color = TFT_YELLOW;
    } else if (extBatteryPerc < 25) {
      batt_color = TFT_RED;
    } else  {
      batt_color = TFT_CYAN;
    }

      //draw bluetooth icon
    sprite.drawWideLine(battery_x - 15, battery_y - 3, battery_x - 15, battery_y + 28, 2, TFT_BLUEBUTTON);
    sprite.drawWideLine(battery_x - 15, battery_y - 3, battery_x - 7, battery_y + 5, 2, TFT_BLUEBUTTON);
    sprite.drawWideLine(battery_x - 25, battery_y + 18, battery_x - 7, battery_y + 5, 2, TFT_BLUEBUTTON);
    sprite.drawWideLine(battery_x - 25, battery_y + 5, battery_x - 7, battery_y + 18, 2, TFT_BLUEBUTTON);
    sprite.drawWideLine(battery_x - 15, battery_y + 28, battery_x - 7, battery_y + 18, 2, TFT_BLUEBUTTON);

    
    sprite.drawRoundRect(battery_x + 2, battery_y, 14, 28, 2, batt_color);
    sprite.fillRect(battery_x + 5, battery_y - 4, 7, 3, batt_color);
    float fillHeight = 24 * extBatteryPerc / 100; // Calculate fill height based on percentage

    sprite.fillRect(battery_x + 4, battery_y + (24 - fillHeight), 10, fillHeight, batt_color);

    sprite.setCursor(battery_x + 24, battery_y + 7, 4);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.printf("%d%%", extBatteryPerc); // Use %% to print the % character
}


void draw_first()
{
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.setFreeFont(&Orbitron_Light_32);
  sprite.setCursor(144, 160);
  sprite.setFreeFont(&FreeSansBold12pt7b);
  sprite.fillRect(114,200,66,66,TFT_RED);
  sprite.fillRect(200,200,66,66,TFT_GREEN);
  sprite.fillRect(286,200,66,66,TFT_BLUE); 
  sprite.setTextSize(1);

  sprite.drawString("powered by SoftRF",233,293,4);
  sprite.drawString(SKYVIEW_FIRMWARE_VERSION,180,400,2);
  lcd_PushColors(6, 0, 466, 466, (uint16_t*)sprite.getPointer());
  for (int i = 0; i <= 255; i++)
  {
    lcd_brightness(i);
    delay(3);
  }
  delay(2000);

}

void TFT_setup(void) {
  pinMode(LCD_EN, OUTPUT);  
  digitalWrite(LCD_EN, HIGH);
  delay(30);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(30);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);
  setupWireIfNeeded(IIC_SDA, IIC_SCL);
  CO5300_init();
  sprite.setColorDepth(16);
  Serial.print("TFT_setup. PSRAM_ENABLE: ");
  Serial.println(sprite.getAttribute(PSRAM_ENABLE));
  sprite.setAttribute(PSRAM_ENABLE, 1);
  // Initialise SPI Mutex
  spiMutex = xSemaphoreCreateMutex();
  if (spiMutex == NULL) {
      Serial.println("Failed to create SPI mutex!");
  }
  lcd_setRotation(0); //adjust #define display_column_offset for different rotations
  lcd_brightness(0); // 0-255    

  Serial.printf("Free heap: %d bytes\n", esp_get_free_heap_size());
  Serial.printf("Largest block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  sprite.createSprite(466, 466);    // full screen landscape sprite in psram
  batterySprite.createSprite(100, 32);
  if (sprite.createSprite(466, 466) == NULL) {
    Serial.println("Failed to create sprite. Not enough memory.");
    delay(5000);
  }
  else {
    Serial.print("TFT_setup. Created Sprite| Free Heap: ");
    Serial.println(esp_get_free_heap_size());
  }

  TFT_view_mode = settings->vmode;
  draw_first();
  TFT_radar_setup();
}

void TFT_loop(void) {
  switch (TFT_view_mode)
  {
  case VIEW_MODE_RADAR:
    TFT_radar_loop();
    break;
  case VIEW_MODE_TEXT:
    TFT_text_loop();
    break;
  case VIEW_MODE_COMPASS:
    TFT_compass_loop();
    break;
  case VIEW_MODE_POWER:
    // Power menu is static, no loop needed
    break;
  default:
    break;
  }

  yield();  // Ensure the watchdog gets reset
  delay(20);
}

void TFT_Mode(boolean next)
{
  if (hw_info.display == DISPLAY_TFT) {

    if (TFT_view_mode == VIEW_MODE_RADAR) {
      if (next) {
        if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
          TFT_view_mode = VIEW_MODE_TEXT;
          if (!bearingSprite.created()) {
            bearingSprite.createSprite(78, 54);
            bearingSprite.setColorDepth(16);
            bearingSprite.setSwapBytes(true);
          }
          TFTTimeMarker = millis() + 1001;
          xSemaphoreGive(spiMutex);
          delay(10);
        } else {
          Serial.println("Failed to acquire SPI semaphore!");
        }
      }
      else {
        if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
          if (settings->compass) {
          TFT_view_mode = VIEW_MODE_COMPASS;
          if (!compasSprite.created()) {
            compasSprite.createSprite(466, 466);
            compasSprite.setColorDepth(16);
            compasSprite.setSwapBytes(true);
          }
          if (!compas2Sprite.created()) {
          compas2Sprite.createSprite(466, 466);
          compas2Sprite.setColorDepth(16);
          compas2Sprite.setSwapBytes(true);
          }
          } else {
            TFT_view_mode = VIEW_MODE_TEXT;
            if (!bearingSprite.created()) {
              bearingSprite.createSprite(78, 54);
              bearingSprite.setColorDepth(16);
              bearingSprite.setSwapBytes(true);
            } 
          }
          lcd_brightness(255);
          TFTrefresh = true;
          TFTTimeMarker = millis() + 1001;
          xSemaphoreGive(spiMutex);
          delay(10);
        } else {
          Serial.println("Failed to acquire SPI semaphore!");
        }
      }

}   else if (TFT_view_mode == VIEW_MODE_TEXT) {
        if (next && settings->compass) {
          if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
            TFT_view_mode = VIEW_MODE_COMPASS;
            if (!compasSprite.created()) {
              compasSprite.createSprite(466, 466);
              compasSprite.setColorDepth(16);
              compasSprite.setSwapBytes(true);
            }
            if (!compas2Sprite.created()) {
              compas2Sprite.createSprite(466, 466);
              compas2Sprite.setColorDepth(16);
              compas2Sprite.setSwapBytes(true);
            }
            if (!compas2Sprite.created()) {
              compas2Sprite.createSprite(466, 466);
              compas2Sprite.setColorDepth(16);
              compas2Sprite.setSwapBytes(true);
            }
            lcd_brightness(220);
            TFTTimeMarker = millis() + 1001;
            TFTrefresh = true;
            xSemaphoreGive(spiMutex);
            delay(10);
            // EPD_display_frontpage = false;
          } else {
            Serial.println("Failed to acquire SPI semaphore!");
          }
        }
        else {
          if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
          TFT_view_mode = VIEW_MODE_RADAR;
            bearingSprite.deleteSprite();
            TFTTimeMarker = millis() + 1001;
            setFocusOn(false); // reset focus
            xSemaphoreGive(spiMutex);
            delay(10);
            // EPD_display_frontpage = false;
          } else {
            Serial.println("Failed to acquire SPI semaphore!");
          }
      }
    }
    else if (TFT_view_mode == VIEW_MODE_COMPASS) {
      if (next) {
        if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
          TFT_view_mode = VIEW_MODE_RADAR;
          compasSprite.deleteSprite();
          compas2Sprite.deleteSprite();
          // sprite.createSprite(466, 466);
          // sprite.setColorDepth(16);
          // sprite.setSwapBytes(true);
          lcd_brightness(255);
          TFTTimeMarker = millis() + 1001;
          // EPD_display_frontpage = false;
          xSemaphoreGive(spiMutex);
          delay(10);
        } else {
          Serial.println("Failed to acquire SPI semaphore!");
        }
      }
      else {
        if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
          TFT_view_mode = VIEW_MODE_TEXT; 
          compasSprite.deleteSprite();
          compas2Sprite.deleteSprite();
          // sprite.createSprite(466, 466);
          // sprite.setColorDepth(16);
          // sprite.setSwapBytes(true);
          // bearingSprite.createSprite(78, 54);
          // bearingSprite.setColorDepth(16);
          // bearingSprite.setSwapBytes(true);
          lcd_brightness(255);
          TFTTimeMarker = millis() + 1001;
          // EPD_display_frontpage = false;
          xSemaphoreGive(spiMutex);
          delay(10);
        } else {
          Serial.println("Failed to acquire SPI semaphore!");
        }
    }
  }
    else if (TFT_view_mode == VIEW_MODE_SETTINGS) {
      if (next) {
        TFT_view_mode = prev_TFT_view_mode;
        EPD_display_frontpage = false;
      }

    }
  }
}

void TFT_Up()
{
  if (hw_info.display == DISPLAY_TFT) {
    switch (TFT_view_mode)
    {
    case VIEW_MODE_RADAR:
      TFT_radar_unzoom();
      break;
    case VIEW_MODE_TEXT:
      TFT_text_prev();
      break;
    case VIEW_MODE_SETTINGS:
      settings_page_number = 2;
      settings_page();
      break;
    default:
      break;
    }
  }
}

void TFT_Down()
{
  if (hw_info.display == DISPLAY_TFT) {
    switch (TFT_view_mode)
    {
    case VIEW_MODE_RADAR:
      TFT_radar_zoom();
      break;
    case VIEW_MODE_TEXT:
      TFT_text_next();
      break;
    case VIEW_MODE_SETTINGS:
      settings_page_number = 1;
      settings_page();
      break;
    default:
      break;
    }
  }
}

void settings_button(uint16_t x, uint16_t y, bool on) {
  if (on) {
    sprite.fillSmoothRoundRect(x, y - 25, 50, 31, 13, TFT_BLUEBUTTON, TFT_BLACK);
    sprite.fillSmoothCircle(x + 33, y - 10, 13, TFT_WHITE, TFT_BLUEBUTTON);
  } else {
    sprite.fillSmoothRoundRect(x, y - 25, 50, 31, 13, TFT_DARKGREY, TFT_BLACK);
    sprite.fillSmoothCircle(x + 17, y - 10, 13, TFT_LIGHTGREY, TFT_BLUEBUTTON);
  }
}

void settings_page_1() {
  if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
    delay(50);
    if (TFT_view_mode != VIEW_MODE_SETTINGS) {
      prev_TFT_view_mode = TFT_view_mode; 
      TFT_view_mode = VIEW_MODE_SETTINGS;}
    uint16_t button_x = 340;
    uint16_t text_y = 0;
  
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setTextDatum(MC_DATUM);
    sprite.setFreeFont(&Orbitron_Light_24);
    sprite.setCursor(160, 40);
    sprite.printf("Settings");

    text_y = 140; //bottom of the text
    sprite.setCursor(button_x - 300, text_y);
    sprite.printf("Traffic filter 500m");
    if ( settings->filter  == TRAFFIC_FILTER_500M) {
      settings_button(button_x, text_y, true);
    } else {
      settings_button(button_x, text_y, false); 
    }

    text_y = 200;
    sprite.setCursor(button_x - 300, text_y);
    sprite.printf("Show Thermals");
    if (settings->show_thermals) {
      settings_button(button_x, text_y, true);
    } else {
      settings_button(button_x, text_y, false);
    }
    
    text_y = 260;
    sprite.setCursor(button_x - 300, text_y);
    sprite.printf("Radar North Up");
    if (settings->orientation == DIRECTION_NORTH_UP) {
      settings_button(button_x, text_y, true);
    } else {
      settings_button(button_x, text_y, false); 
    }

    text_y = 320;
    sprite.setCursor(button_x - 300, text_y);
    sprite.printf("Show Labels");

    if (isLabels) {
      settings_button(button_x, text_y, true);

    } else {
      settings_button(button_x, text_y, false);

    }


    // Page indicator
    sprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
    sprite.setFreeFont(&Orbitron_Light_24);
    sprite.setCursor(210, 380);
    sprite.printf("1/2");

    // Back button
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setCursor(button_x - 130, 440);
    sprite.printf("BACK");
    sprite.fillTriangle(180, 430, 197, 417, 197, 443, TFT_BLUEBUTTON);
    sprite.fillTriangle(160, 430, 180, 417, 180, 443, TFT_BLUEBUTTON);

    lcd_PushColors(display_column_offset, 0, 466, 466, (uint16_t*)sprite.getPointer());
    lcd_brightness(255);
    xSemaphoreGive(spiMutex);
  } else {
    Serial.println("Failed to acquire SPI semaphore!");
  }
}

void settings_page_2() {
  if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
    delay(50);
    if (TFT_view_mode != VIEW_MODE_SETTINGS) {
      prev_TFT_view_mode = TFT_view_mode;
      TFT_view_mode = VIEW_MODE_SETTINGS;
    }
    uint16_t button_x = 340;
    uint16_t text_y = 0;

    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setTextDatum(MC_DATUM);
    sprite.setFreeFont(&Orbitron_Light_24);
    sprite.setCursor(160, 40);
    sprite.printf("Settings");

    text_y = 140;
    sprite.setCursor(button_x - 300, text_y);
    sprite.printf("Compass Page");
    if (settings->compass) {
      settings_button(button_x, text_y, true);
    } else {
      settings_button(button_x, text_y, false);
    }

    text_y = 200;
    sprite.setCursor(button_x - 300, text_y);
    sprite.printf("Voice Alerts");
    if (settings->voice_alerts) {
      settings_button(button_x, text_y, true);
    } else {
      settings_button(button_x, text_y, false);
    }

    text_y = 260;
    sprite.setCursor(button_x - 300, text_y);
    sprite.printf("Demo Mode");
    if (settings->demo_mode) {
      settings_button(button_x, text_y, true);
    } else {
      settings_button(button_x, text_y, false);
    }

    text_y = 340;
    sprite.setCursor(button_x - 240, 320);
    sprite.printf("Power Options");
    sprite.setSwapBytes(true);
    sprite.pushImage(button_x, 290, 48, 47, power_button_small);

    // Page indicator
    sprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
    sprite.setFreeFont(&Orbitron_Light_24);
    sprite.setCursor(210, 380);
    sprite.printf("2/2");

    // Back button
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setCursor(button_x - 130, 440);
    sprite.printf("BACK");
    sprite.fillTriangle(180, 430, 197, 417, 197, 443, TFT_BLUEBUTTON);
    sprite.fillTriangle(160, 430, 180, 417, 180, 443, TFT_BLUEBUTTON);

    lcd_PushColors(display_column_offset, 0, 466, 466, (uint16_t*)sprite.getPointer());
    lcd_brightness(255);
    xSemaphoreGive(spiMutex);
  } else {
    Serial.println("Failed to acquire SPI semaphore!");
  }
}

void settings_page() {
  Serial.print("Previous view mode was: ");
  Serial.println(prev_TFT_view_mode);
  // Route to appropriate page based on settings_page_number
  if (settings_page_number == 1) {
    settings_page_1();
  } else {
    settings_page_2();
  }
}

void TFT_DoubleClick()
{
  if (TFT_view_mode == VIEW_MODE_POWER) {
    // Double click when menu is showing = Full shutdown
    ESP32_TFT_fini("FULL POWER OFF");
    power_off();
  } else {
    // Double click - go to settings page
    settings_page();
  }
}

void TFT_show_power_menu()
{
  if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
    // Set to power menu view mode
    TFT_view_mode = VIEW_MODE_POWER;

    sprite.fillSprite(TFT_BLACK);

    // Title - smaller font and lower position
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    sprite.setFreeFont(&Orbitron_Light_24);
    const char* title = "POWER OPTIONS";
    uint16_t title_wd = sprite.textWidth(title);
    sprite.setCursor(LCD_WIDTH / 2 - title_wd / 2, 100);
    sprite.printf(title);

    // Sleep option (top button) - single click
    sprite.fillRoundRect(83, 160, 300, 80, 10, TFT_BLUEBUTTON);
    sprite.setTextColor(TFT_WHITE, TFT_BLUEBUTTON);
    sprite.setFreeFont(&Orbitron_Light_24);
    const char* sleep_txt = "SLEEP";
    uint16_t sleep_wd = sprite.textWidth(sleep_txt);
    sprite.setCursor(LCD_WIDTH / 2 - sleep_wd / 2, 210);
    sprite.printf(sleep_txt);

    // Full Shutdown option (bottom button) - double click
    sprite.fillRoundRect(83, 260, 300, 80, 10, TFT_RED);
    sprite.setTextColor(TFT_WHITE, TFT_RED);
    const char* shutdown_txt = "FULL SHUTDOWN";
    uint16_t shutdown_wd = sprite.textWidth(shutdown_txt);
    sprite.setCursor(LCD_WIDTH / 2 - shutdown_wd / 2, 310);
    sprite.printf(shutdown_txt);

    lcd_PushColors(display_column_offset, 0, 466, 466, (uint16_t*)sprite.getPointer());
    lcd_brightness(255);

    xSemaphoreGive(spiMutex);
  } else {
    Serial.println("Failed to acquire SPI semaphore!");
  }
}

#endif /* USE_TFT */