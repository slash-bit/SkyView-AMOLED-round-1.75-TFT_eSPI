#include <Arduino.h>
// #include "TouchDrvCST92xx.h"
#include <driver/touch/TouchDrvCSTXXX.hpp>
#include "../pins_config.h"
#include "TouchHelper.h"
#include "TFTHelper.h"
#include "NMEAHelper.h"
#include "View_Radar_TFT.h"
#include "View_Text_TFT.h"
#include "Platform_ESP32.h"
#include "SkyView.h"
#include "EEPROMHelper.h"
#include <TimeLib.h>
#include "TrafficHelper.h"
#include "BatteryHelper.h"
// Create an instance of the CST9217 class

TouchDrvCST92xx touchSensor;

uint8_t touchAddress = 0x5A;
extern int TFT_view_mode;

int16_t endX = -1, endY = -1;
static int16_t startX = -1, startY = -1;
static uint32_t startTime = 0;
int16_t currentX[5], currentY[5];

// Task Handle
TaskHandle_t touchTaskHandle = NULL;

bool IIC_Interrupt_Flag = false;
unsigned long lastTapTime = 0;
unsigned long debounceDelay = 100; // in milliseconds

bool isLabels = true;
// bool show_compass = false;
extern bool isLocked;

void Touch_2D_Unrotate(float &tX, float &tY) {
    float angleRad = D2R * ThisAircraft.Track;
    float trSin = sin(angleRad);
    float trCos = cos(angleRad);

    float tTemp = tX * trCos + tY * trSin;
    tY = -tX * trSin + tY * trCos;
    tX = tTemp;
}


void findTouchedTarget(int rawTouchX, int rawTouchY) {
  if (TFT_view_mode != VIEW_MODE_RADAR) return;
  int zoom = getCurrentZoom();
  int range = 3000;  // default
  // Convert touch to center-relative coordinates (pixels)
  float touchX = rawTouchX - (LCD_WIDTH / 2);
  float touchY = -(rawTouchY - (LCD_HEIGHT / 2));
  // Serial.printf("Touch coordinates: (%f, %f)\n", touchX, touchY);

  // Rotate to NORTH_UP if needed
  if (settings->orientation == DIRECTION_TRACK_UP) {
    Touch_2D_Unrotate(touchX, touchY);
    // Serial.printf("Unrotated touch coordinates: (%f, %f)\n", touchX, touchY);
  }

  // Get current range in meters
  if (settings->units == UNITS_METRIC || settings->units == UNITS_MIXED) {
    switch (zoom) {
      case ZOOM_LOWEST:  range = 9000; break;
      case ZOOM_LOW:     range = 6000; break;
      case ZOOM_HIGH:    range = 900;  break;
      case ZOOM_MEDIUM:
      default:           range = 3000; break;
    }
  } else {
    switch (zoom) {
      case ZOOM_LOWEST:  range = 9260; break;
      case ZOOM_LOW:     range = 4630; break;
      case ZOOM_HIGH:    range = 926;  break;
      case ZOOM_MEDIUM:
      default:           range = 1852; break;
    }
  }

  // Convert pixel distances to meters
  float radius = LCD_WIDTH / 2.0f - 2.0f;
  float scale = (float)range / radius;  // meters per pixel
  float touchXMeters = touchX * scale;
  float touchYMeters = touchY * scale;
  int touchHitRadius = range / 10; // 10% of the range as hit radius

  // Serial.printf("Touch in meters: (%f, %f)\n", touchXMeters, touchYMeters);

  for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
    if (Container[i].ID == 0) continue;
    if ((now() - Container[i].timestamp) > TFT_EXPIRATION_TIME) continue;

    // Serial.printf("Checking target ID: %06X\n", Container[i].ID);
    // Serial.printf("Target coordinates: (%f, %f)\n", Container[i].RelativeEast, Container[i].RelativeNorth);

    float dx = Container[i].RelativeEast - touchXMeters;
    float dy = Container[i].RelativeNorth - touchYMeters;

    if ((dx * dx + dy * dy) < (touchHitRadius * touchHitRadius)) {
      // Serial.printf("Touched target ID: %06X at (%f, %f)\n", Container[i].ID, Container[i].RelativeEast, Container[i].RelativeNorth);
      // TODO: handle selection
      isLocked = true;
      setFocusOn(true, Container[i].ID);
      delay(100); // Debounce delay
      TFT_Mode(true); // Switch to text view
      break;
    }
  }
}



void touchWakeUp() {
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(30);
  digitalWrite(TOUCH_RST, HIGH);
  delay(100);
  touchSensor.wakeup();
}

void Touch_setup() {
  
    attachInterrupt(TOUCH_INT, []()
    { IIC_Interrupt_Flag = true; }, FALLING);

        touchSensor.setPins(TOUCH_RST, TOUCH_INT);
  if (touchSensor.begin(Wire, touchAddress, IIC_SDA, IIC_SCL) == false)
  {
      Serial.println("CST9217 initialization failed");
  }
  else
  {
      Serial.print("Model :");
      Serial.println(touchSensor.getModelName());
      touchSensor.setMaxCoordinates(466, 466); // Set touch max xy
      touchSensor.setMirrorXY(true, true); 
  }
  xTaskCreatePinnedToCore(touchTask, "Touch Task", 4096, NULL, 1, &touchTaskHandle, 1);
  }

void tapHandler(int x, int y) {
  // Touch coordinates are now properly mirrored via setMirrorXY() in Touch_setup()
  Serial.println("Tap detected at coordinates: " + String(x) + ", " + String(y));

  // Handle power menu taps
  if (TFT_view_mode == VIEW_MODE_POWER) {
    // Sleep button (top button: 83,160 to 383,240)
    if (x > 83 && x < 383 && y > 160 && y < 240) {
      Serial.println("Sleep selected from power menu");
      shutdown("SLEEP");
      return;
    }
    // Full Shutdown button (bottom button: 83,260 to 383,340)
    else if (x > 83 && x < 383 && y > 260 && y < 340) {
      Serial.println("Full Shutdown selected from power menu");
      ESP32_TFT_fini("FULL POWER OFF");
      power_off();
      return;
    }
    // Any other tap dismisses the menu and restores previous view
    else {
      Serial.println("Power menu dismissed");
      TFT_view_mode = VIEW_MODE_SETTINGS;
      settings_page();
      return;
    }
  }

  bool hasFix = settings->protocol == PROTOCOL_NMEA  ? isValidGNSSFix() : false;
  if (x > 290 && x < 400 && y > 360 && y < 466
    && (TFT_view_mode == VIEW_MODE_TEXT || (TFT_view_mode == VIEW_MODE_RADAR && !hasFix))) {
    Serial.println("Going to SettingsPage ");
    settings_page();
  }

  else if (x > 160 && x < 330 && y > 410 && y < 466
    && TFT_view_mode == VIEW_MODE_SETTINGS) {
    // Back button - reset to page 1 when leaving settings
    extern int settings_page_number;
    settings_page_number = 1;
    TFT_Mode(true);
  } 
  // Settings Page 1 tap handlers
  else if (x > 320 && x < 420 && y > 110 && y < 170
    && TFT_view_mode == VIEW_MODE_SETTINGS) {
    extern int settings_page_number;
    if (settings_page_number == 1) {
      // Traffic Filter +- 500m
      if (settings->filter == TRAFFIC_FILTER_500M) {
        settings->filter = TRAFFIC_FILTER_OFF;
      } else {
        settings->filter = TRAFFIC_FILTER_500M;
      }
      settings_page();
    }
      else if (settings_page_number == 2) {
      // Compass Page toggle
      settings->compass = !settings->compass;
      settings_page();
    }
  }
  else if (x > 320 && x < 420 && y > 170 && y < 230
    && TFT_view_mode == VIEW_MODE_SETTINGS) {
    extern int settings_page_number;
    if (settings_page_number == 1) {
      // Show Thermals toggle
      settings->show_thermals = !settings->show_thermals;
      settings_page();
    } else if (settings_page_number == 2) {
      // Voice Alerts toggle
      settings->voice_alerts = !settings->voice_alerts;
      EEPROM_store();
      Serial.printf("Voice alerts toggled: %s\n", settings->voice_alerts ? "ON" : "OFF");
      settings_page();
    }
  }
  else if (x > 320 && x < 420 && y > 227 && y < 290
    && TFT_view_mode == VIEW_MODE_SETTINGS) {
    extern int settings_page_number;
    if (settings_page_number == 1) {
      // Radar Orientation North Up / Track Up
      if (settings->orientation == DIRECTION_NORTH_UP) {
        settings->orientation = DIRECTION_TRACK_UP;
      } else {
        settings->orientation = DIRECTION_NORTH_UP;
      }
      settings_page();
    } else if (settings_page_number == 2) {
      // Demo mode toggle - just toggle the flag and reboot
      // The boot synchronization code will set the correct connection mode
      settings->demo_mode = !settings->demo_mode;

      // Save to EEPROM first
      EEPROM_store();
      Serial.printf("Demo mode toggled: %s\n", settings->demo_mode ? "ON" : "OFF");

      // Redraw settings page to show the new toggle state
      settings_page();
      delay(500);  // Let user see the toggle change

      // Display reboot message
      if (settings->demo_mode) {
        Serial.println("Demo mode will be enabled on next boot");
        TFT_radar_Draw_Message("Demo Mode Enabled", "Rebooting...");
      } else {
        Serial.println("Demo mode will be disabled on next boot");
        TFT_radar_Draw_Message("Demo Mode Disabled", "Rebooting...");
      }

      delay(2000);  // Show message for 2 seconds
      ESP.restart();  // Reboot - settings will be applied during boot
    }
  }
  else if (x > 320 && x < 420 && y > 290 && y < 420
    && TFT_view_mode == VIEW_MODE_SETTINGS) {
    extern int settings_page_number;
    if (settings_page_number == 1) {
      // Show Labels toggle
      isLabels = !isLabels;
      settings->show_labels = isLabels;
      settings_page();
    } else if (settings_page_number == 2) {
        TFT_show_power_menu();
    }
  }
  else if (x > 400 && x < 466 && y > 170 && y < 280 && TFT_view_mode == VIEW_MODE_TEXT) {
    //Togle Average Vario
    if (!show_avg_vario) {
      show_avg_vario = true;
    }
    else {
      show_avg_vario = false;
    }
    TFTTimeMarker = 0; // Force update of the display
  }
  else if (x > 210 && x < 250 && y > 116 && y < 176 && TFT_view_mode == VIEW_MODE_TEXT) {
    // Toggle RSSI display mode (numeric vs icon)
    toggleRssiDisplay();
    Serial.println("RSSI display mode toggled");
  }
  else if (TFT_view_mode == VIEW_MODE_RADAR) {
    findTouchedTarget(x, y);

  } else {
    Serial.println("No Tap match found...");
  }


}


void touchTask(void *parameter) {
    

    while(true) {
    
  
    if (IIC_Interrupt_Flag) {
    //   Serial.println("Interrupt triggered!");
        IIC_Interrupt_Flag = false; // Reset interrupt flag
      uint8_t points = touchSensor.getPoint(currentX, currentY, 1); // Read single touch point
  
      if (points > 0) {
        // Record the starting touch position and time
        if (startX == -1 && startY == -1) {
          startX = currentX[0];
          startY = currentY[0];
          startTime = millis();

        }
  
        // Continuously update the end position
        endX = currentX[0];
        endY = currentY[0];
      } else {
        // If no more points are detected, process swipe
        if (startX != -1 && startY != -1) {
          uint32_t duration = millis() - startTime;

  
          int16_t deltaX = endX - startX;
          int16_t deltaY = endY - startY;
  
          // Swipe detection logic
          if (duration < 500) { // Limit gesture duration
            if (abs(deltaX) > abs(deltaY)) { // Horizontal swipe

                // Normal view mode navigation
                if (deltaX < -30) {
                  Serial.println("Swipe Left");
                  TFT_Mode(true);
                } else if (deltaX > 30) {
                  Serial.println("Swipe Right");
                  TFT_Mode(false);
                }
            } else if (abs(deltaX) < abs(deltaY)) { // Vertical swipe
                if (deltaY < -50) {
                  Serial.println("Swipe Up");
                  TFT_Up();

                } else if (deltaY > 50) {
                  Serial.println("Swipe Down");
                  TFT_Down();
                }
              } else if (abs(deltaX) < 50 && abs(deltaY) < 50) {
                unsigned long currentTime = millis();
                if (currentTime - lastTapTime >= debounceDelay) {
                  lastTapTime = currentTime;
                  // Serial.println("Tap");
                  delay(100); // Debounce delay
                  tapHandler(endX, endY); // Call tap handler with coordinates
                }
              }
            }
            else if (duration > 500 && duration < 2000 && abs(deltaX) < 50 && abs(deltaY) < 50) {
              Serial.println("Long Press");
            }
          // Reset variables for next swipe detection
          startX = startY = -1;
          startTime = 0;
          endX = endY = -1;
        }
      }
      
    }
  
    delay(50); // Polling delay
  }
}
  






