#include "SkyView.h"
#include "TFTHelper.h"
#include "NMEAHelper.h"
#include "TrafficHelper.h"
#include "EEPROMHelper.h"
#include "Compas466x466.h"
#include "SatDish.h"
#include "Speed.h"
#include "altitude2.h"

extern xSemaphoreHandle spiMutex;
extern bool TFTrefresh;
extern TFT_eSPI tft;
// extern TFT_eSprite sprite;
extern TFT_eSprite compasSprite;
extern TFT_eSprite compas2Sprite;
    

void TFT_compass_loop() {
    if (TFTrefresh && settings->compass) {
        Serial.println("TFT_compass_display");
        if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
            compasSprite.fillSprite(TFT_BLACK);
            // sprite.fillSprite(TFT_BLACK);
            compasSprite.pushImage(0, 0, 466, 466, Compas466x466);
            compasSprite.setPivot(233, 233);
            
            compas2Sprite.fillSprite(TFT_BLACK);
            
            compas2Sprite.fillTriangle(215, 125, 235, 90, 255, 125, TFT_WHITE);
            compas2Sprite.setSwapBytes(true);
            compas2Sprite.pushImage(180, 230, 32, 24, Speed);
            compas2Sprite.setCursor(220, 240, 4);
            switch (settings->units) {
                case UNITS_METRIC:
                    compas2Sprite.printf("%d", (int)(ThisAircraft.GroundSpeed * 1.852));  // knots to km/h
                    break;
                case UNITS_IMPERIAL:
                case UNITS_MIXED:
                    compas2Sprite.printf("%d", (int)(ThisAircraft.GroundSpeed * 1.15078));  // knots to mph
                    break;
                default:
                    compas2Sprite.printf("%d", (int)(ThisAircraft.GroundSpeed));  // knots
                    break;
            }
            compas2Sprite.pushImage(135, 280, 32, 32, SatDishpng);
            compas2Sprite.setCursor(180, 290, 4);
            if (nmea.satellites.isValid()) {
                int satelliteCount = nmea.satellites.value();
                compas2Sprite.printf("%d", satelliteCount);
            }
            else {
                compas2Sprite.setTextColor(TFT_RED, TFT_BLACK);
                compas2Sprite.printf("NO FIX");
                compas2Sprite.setTextColor(TFT_WHITE, TFT_BLACK);
            }

            compas2Sprite.pushImage(255, 280, 32, 32, altitude2);
            compas2Sprite.setCursor(300, 290, 4);
            switch (settings->units) {
                case UNITS_METRIC:
                    compas2Sprite.printf("%d m", (int)(ThisAircraft.altitude));
                    break;
                case UNITS_IMPERIAL:
                case UNITS_MIXED:
                    compas2Sprite.printf("%d ft", (int)(ThisAircraft.altitude * 3.28084));
                    break;
                default:
                    compas2Sprite.printf("%d ft", (int)(ThisAircraft.altitude * 3.28084));
                    break;
            }
            compas2Sprite.setCursor(190, 160, 7);
            compas2Sprite.printf("%d", ThisAircraft.Track);
            // compas2Sprite.pushToSprite(&sprite, 0, 0, TFT_BLACK);

            compas2Sprite.setPivot(233, 233);
            compasSprite.pushRotated(&compas2Sprite, 360 - ThisAircraft.Track, TFT_BLACK);

            lcd_PushColors(6, 0, 466, 466, (uint16_t*)compas2Sprite.getPointer());
            
            xSemaphoreGive(spiMutex);
            delay(100);
          } else {
            Serial.println("Failed to acquire SPI semaphore!");
          }
          TFTrefresh = false;
        //   TFTTimeMarker = millis();
    }
}